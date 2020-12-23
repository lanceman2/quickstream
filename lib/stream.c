// For functions related to connecting blocks via a stream connection.
// These streams we refer to are a way for blocks to exchange data that is
// like a continuous stream, like a very long array that functions just get
// pointers to.  It does not require a constant rate for quickstream, we
// let the hardware do whatever it requires.  If the hardware is required
// to "stream" data at a constant we let it.
//
// Parameter connection are another kind of connection.  Parameter data is
// exchanged in small discrete chunks like parameters copied into the
// parameter arguments of a function.
//

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/builder.h"
#include "../include/quickstream/block.h"

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "graph.h"
#include "trigger.h"
#include "threadPool.h"
#include "triggerJobsLists.h"



int qsBlockConnect(struct QsBlock *_from, struct QsBlock *_to,
        uint32_t fromPortNum, uint32_t toPortNum) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    DASSERT(_from);
    DASSERT(_to);
    DASSERT(!_from->isSuperBlock);
    DASSERT(!_to->isSuperBlock);

    if(_from->isSuperBlock || _to->isSuperBlock) {
        ASSERT(0, "Write code to connect SuperBlocks by a stream");
        return -1; // error
    }

    if(_to == _from) {
        ERROR("A block cannot connect to itself");
        return -2; // error
    }

    struct QsSimpleBlock *to = (struct QsSimpleBlock *) _to;
    struct QsSimpleBlock *from = (struct QsSimpleBlock *) _from;

    DASSERT(from->flow, "Block \"%s\" does not have a flow() function",
                    _from->name);
    DASSERT(to->flow, "Block \"%s\" does not have a flow() function",
                    _to->name);

    if(!to->flow || !from->flow) {
        if(!from->flow)
            ERROR("Block \"%s\" does not have a flow() function",
                    _from->name);
        if(!to->flow)
            ERROR("Block \"%s\" does not have a flow() function",
                    _to->name);
        return -3;
    }

    struct QsGraph *g = _from->graph;
    DASSERT(g);
    DASSERT(_to->graph == g);
    if(_to->graph != g || !g) {
        ERROR("Blocks are not from the same graph");
        return -4; // error
    }

    // Check that the input port is not occupied already.
    if(to->numInputs > toPortNum && to->inputs[toPortNum].feederBlock) {
        // There is a block that is feeding this port already.
        // An input port cannot have more than one connection to it.
        ERROR("Block %s input port %" PRIu32
                " is feed already by block %s from its' %" PRIu32
                " output port", _to->name, toPortNum,
                to->inputs[toPortNum].feederBlock->block.name,
                to->inputs[toPortNum].outputPortNum);
        return -5; // error
    }
    // It's okay if an output port has a connection already.

    // Make sure that from->outputs has the needed number of ports.
    // Port numbers start at 0, just like the array index that it is.
    if(from->numOutputs <= fromPortNum) {
        // We need to add an output port.  We'll check that all output
        // ports in the array get used before we start the streams.
        uint32_t oldNum = from->numOutputs;
        from->numOutputs = fromPortNum + 1;
        from->outputs = realloc(from->outputs,
                from->numOutputs*sizeof(*from->outputs));
        ASSERT(from->outputs, "realloc(,%zu) failed",
                from->numOutputs*sizeof(*from->outputs));
        // Realloc() does not initialize, so we do here, just the ones
        // that we just added.  Any old memory is saved by realloc().
        memset(from->outputs + oldNum, 0,
                (from->numOutputs - oldNum)*sizeof(*to->outputs));
        // Initialize all the outputs that we just added with the
        // output defaults that are not zero.
        for(uint32_t i = oldNum - 1; i < from->numOutputs; ++i)
            from->outputs[i].maxWrite = QS_DEFAULT_OUTPUT_MAXWRITE;
    }

    // Make sure that to->inputs has the needed number of ports.
    // Port numbers start at 0, just like the array index that it is.
    if(to->numInputs <= toPortNum) {
        // We need to add an input port.  We'll check that all input
        // ports in the array get used before we start the streams.
        uint32_t oldNum = to->numInputs;
        to->numInputs = toPortNum + 1;
        // This will likely change the addresses of the inputs that the
        // outputs need to know about.
        to->inputs = realloc(to->inputs,
                to->numInputs*sizeof(*to->inputs));
        ASSERT(to->inputs, "realloc(,%zu) failed",
                to->numInputs*sizeof(*to->inputs));
        // Realloc() does not initialize, so we do here, just the ones
        // that we just added.  Any old memory is saved by realloc().
        memset(to->inputs + oldNum, 0,
                (to->numInputs - oldNum)*sizeof(*to->inputs));
        // Initialize all the inputs that we just added with the
        // input defaults that are not zero.
        for(uint32_t i = oldNum - 1; i < to->numInputs; ++i) {
            struct QsInput *input = to->inputs + i;
            input->threshold = QS_DEFAULT_INPUT_THRESHOLD;
            input->maxRead = QS_DEFAULT_INPUT_MAXREAD;
        }
    }

    // Now connect the blocks.

    struct QsOutput *output = from->outputs + fromPortNum;
    struct QsInput *input = to->inputs + toPortNum;

    // Add a count of the number of inputs that an output will feed stream
    // data.
    //
    // Each output port keeps a list of the to block inputs which we will
    // allocate just before flow start.  We cannot allocate them now since
    // the addresses of the inputs can be changed each time a connection
    // is added (realloc()) above.  We just count them for now.
    ++output->numInputs;
    // We make this array of pointers from the output to the inputs just
    // before we run the flow, and remove it just after.
    DASSERT(output->inputs == 0);

    input->block = to;
    input->feederBlock = from;
    input->inputPortNum = toPortNum;
    input->outputPortNum = fromPortNum;

    return 0; // success
}


// We only need the block with the input port to remove a connection,
// because each input port can only have one connection.
//
int qsBlockDisconnect(struct QsBlock *b, uint32_t inputPortNum) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    DASSERT(b);
    DASSERT(b->graph);
    DASSERT(!b->isSuperBlock);

    ASSERT(b->graph->flowState == QsGraphPaused);

    if(b->isSuperBlock) {
        ASSERT(0, "Write code to connect SuperBlocks by a stream");
        return -1; // error
    }

    // Verify there is a connection.
    struct QsSimpleBlock *to = (struct QsSimpleBlock *) b;


    if(to->numInputs <= inputPortNum) {
        ERROR("There is no connection to block %s into port %" PRIu32,
                b->name, inputPortNum);
        return -2;
    }

    struct QsSimpleBlock *from = to->inputs[inputPortNum].feederBlock;
    DASSERT(from);
    uint32_t outputPortNum = to->inputs[inputPortNum].outputPortNum;
    DASSERT(from->numOutputs > outputPortNum);

    struct QsOutput *output = from->outputs + outputPortNum;

    // output->inputs array of pointers gets allocated just before running
    // the stream and destroyed just after.
    DASSERT(output->inputs == 0);
    // There should be no ring buffer at this time either.
    DASSERT(output->buffer == 0);
    // This input must be counted in the output thingy.
    DASSERT(output->numInputs);

    --output->numInputs;

    if(output->numInputs == 0) {

        // We cannot shuffle outputs array[] so it has no empty elements
        // in it, because the user specified the port numbers.  It's up to
        // the user to not leave empty slots in this array.  If this array
        // element is not on the end then we just zero it out.  Ya, in the
        // case where we end up free this memory this is wasted effort,
        // but this is simpler.
        memset(output, 0, sizeof(*output));

        // Only if the last element in the array is the one being nulled,
        // do we need to remove it.
        if(from->numOutputs == outputPortNum + 1) {
            // This is the end element in the outputs[] array.
            // Figure out how many elements are zeroed after this one.
            uint32_t newNumOutputs = from->numOutputs - 1;
            for(;newNumOutputs; --newNumOutputs)
                if(from->outputs[newNumOutputs - 1].numInputs)
                    break;
            from->numOutputs = newNumOutputs;

            if(newNumOutputs) {
                from->outputs = realloc(from->outputs,
                        newNumOutputs*sizeof(*from->outputs));
                ASSERT(from->outputs, "realloc(,%zu) failed",
                        newNumOutputs*sizeof(*from->outputs));
            } else {
                // We will have no more outputs
                free(from->outputs);
                from->outputs = 0;
            }
        }
    }

    // 2. Remove "to" input data from port inputPortNum
    //
    struct QsInput *input = to->inputs + inputPortNum;

    // inputs can only have one input connection, so we will null
    // that connection, no ifs about it.
    memset(input, 0, sizeof(*input));

    // Only if the last element in the array is the one being nulled, do
    // we need to remove it.
    if(to->numInputs == inputPortNum + 1) {
        // This is the end element in the inputs[] array.
        // Figure out how many elements are zeroed after this one.
        uint32_t newNumInputs = to->numInputs - 1;
        for(;newNumInputs; --newNumInputs)
            if(to->inputs[newNumInputs - 1].feederBlock)
                break;

        to->numInputs = newNumInputs;

        if(newNumInputs) {
            to->inputs = realloc(to->inputs,
                    newNumInputs*sizeof(*to->inputs));
            ASSERT(to->inputs, "realloc(,%zu) failed",
                    newNumInputs*sizeof(*to->inputs));
        } else {
            // We have no more inputs.
            free(to->inputs);
            to->inputs = 0;
        }
    }

    return 0; // success
}

void RemoveOutputInputsArray(struct QsGraph *g) {

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        if(smB->numOutputs == 0) continue;
        DASSERT(smB->outputs);
        for(uint32_t i = smB->numOutputs - 1; i != -1; --i) {
            struct QsOutput *output = smB->outputs + i;
            if(output->numInputs == 0) continue;
            DASSERT(output->inputs);
#ifdef DEBUG
            memset(output->inputs, 0,
                    output->numInputs*sizeof(*output->inputs));
#endif
            free(output->inputs);
            output->inputs = 0;
        }
    }
}


// Return 0 on success else returns the number of outputs without inputs.
uint32_t MakeOuputInputsArray(struct QsGraph *g) {

    for(struct QsBlock *b0 = g->firstBlock; b0; b0 = b0->next) {
        if(b0->isSuperBlock) continue;
        struct QsSimpleBlock *smB0 = (struct QsSimpleBlock *) b0;
        if(smB0->numOutputs == 0) {
            DASSERT(smB0->outputs == 0);
            continue;
        }
        DASSERT(smB0->outputs);
        for(uint32_t i = smB0->numOutputs - 1; i != -1; --i) {
            struct QsOutput *output = smB0->outputs + i;
            if(output->numInputs == 0) continue;
            output->inputs =
                calloc(1, output->numInputs*sizeof(*output->inputs));
            ASSERT(output->inputs, "malloc(%zu) failed",
                    output->numInputs*sizeof(*output->inputs));
        }


        for(struct QsBlock *b1 = g->firstBlock; b1; b1 = b1->next) {
            if(b1->isSuperBlock) continue;
            struct QsSimpleBlock *smB1 = (struct QsSimpleBlock *)b1;
            uint32_t j = smB1->numInputs - 1;
            for(;j != -1; --j) {
                if(smB1->inputs[j].feederBlock == smB0) {
                    DASSERT(smB1->inputs[j].inputPortNum == j);
                    uint32_t outputPortNum = smB1->inputs[j].outputPortNum;
                    DASSERT(outputPortNum < smB0->numOutputs);
#ifdef DEBUG
                    uint32_t numInputs =
                        smB0->outputs[outputPortNum].numInputs;
                    DASSERT(numInputs);
#endif
                    // Get the array of pointers to inputs in this output.
                    struct QsInput **oInputs =
                        smB0->outputs[outputPortNum].inputs;
                    // Find an input pointer that is 0 (unused), ...
                    while(*oInputs != 0) {
                        // We should not overrun the array.
                        DASSERT(--numInputs);
                        ++oInputs;
                    }
                    // and set its' value.
                    *oInputs = smB1->inputs + j;
                    DASSERT((*oInputs)->outputPortNum == outputPortNum);
                }
            }
        }
    }

    int numError = 0;

    // Check that all outputs are connected to inputs.
    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        for(uint32_t i = smB->numOutputs - 1; i != -1; --i)
            if((smB->outputs + i)->numInputs == 0) {
                ERROR("Block \"%s\" has outputs not connected",
                        b->name);
                ++numError;
                continue;
            }
    }

    if(numError) {
        RemoveOutputInputsArray(g);
        return numError;
    }

    // Check that all inputs are connected to inputs.
    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        for(uint32_t i = smB->numInputs - 1; i != -1; --i)
            if((smB->inputs + i)->feederBlock == 0) {
                ERROR("Block \"%s\" has inputs not connected",
                        b->name);
                ++numError;
                continue;
            }
    }

    if(numError)
        RemoveOutputInputsArray(g);

    return numError;
}


// Returns true if there is a loop.
static bool
HaveLoops(struct QsSimpleBlock *b, uint32_t numFilters,
        uint32_t count) {

    for(int32_t i = b->numOutputs - 1; i != -1; --i) {
        ++count;
        if(count > numFilters)
            // We have been to count blocks greater than the number of
            // filters, therefore we are looping.  The recursive function
            // would blow the stack if not for this return.
            return true;
        for(int32_t j = b->outputs[i].numInputs - 1; j != -1; --j)
            return HaveLoops(b->outputs[i].inputs[j]->block,
                    numFilters, count);
    }
    return false;
}


static inline
int IsSource(const struct QsSimpleBlock *b) {
    // Sources have no input and some output:
    return (b->numInputs == 0 && b->numOutputs);
}


// Allocate stream ring buffers and ...
int StreamsStart(struct QsGraph *g) {

    DASSERT(g);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    // Find all sources
    uint32_t numSources = 0;
    DASSERT(g->sources == 0);

    g->numFilters = 0;

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        if(IsSource((struct QsSimpleBlock *)b))
            ++numSources;
        if(((struct QsSimpleBlock *)b)->numInputs ||
                ((struct QsSimpleBlock *)b)->numOutputs)
            ++g->numFilters; // has input and/or output
    }

    if(g->numFilters == 0) return 0; // success


    if(numSources == 0) {
        ERROR("There are %" PRIu32 " filter blocks and"
                " none are sources", g->numFilters);
        return -1;
    }

    if(MakeOuputInputsArray(g)) return -1;


    uint32_t i = 0;

    g->sources =
            malloc((numSources+1)*sizeof(*g->sources));
    ASSERT(g->sources, "malloc(%zu) failed",
            (numSources+1)*sizeof(*g->sources));

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        if(IsSource((struct QsSimpleBlock *) b))
            g->sources[i++] = (struct QsSimpleBlock *)b;
    }
    g->sources[i] = 0; // zero terminate

    // dummy iterator, s
    struct  QsSimpleBlock **s = g->sources;
    
    // Check the stream graph has no loops.
    struct QsSimpleBlock *b = *s;
    for(; b; b = *(++s))
        if(HaveLoops(b, g->numFilters, 0))
            break;
    if(b) {
#ifdef DEBUG
        memset(g->sources, 0, numSources*sizeof(*g->sources));
#endif
        free(g->sources);
        g->sources = 0;
        g->numFilters = 0;
        RemoveOutputInputsArray(g);
        ERROR("There are loops starting from block \"%s\"",
                b->block.name);
        return -1;
    }


    // Allocate buffers and map ring buffers
    CreateRingBuffers(g);


    return 0; // success
}


// Free the stream ring buffers and ...
void StreamStop(struct QsGraph *g) {

    DASSERT(g);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    if(g->numFilters == 0) {
        DASSERT(g->sources == 0);
        return;
    }

    DestroyRingBuffers(g);

    RemoveOutputInputsArray(g);

    DASSERT(g->sources);
#ifdef DEBUG
    for(struct QsSimpleBlock **s = g->sources; *s; ++(s))
        *s = 0;
#endif
    free(g->sources);
    g->sources = 0;
    g->numFilters = 0;
}
