// Makes the ring buffers.  The hard part is figuring out the size of
// them.
//
// It doesn't appear that GNUradio varies the length of the ring buffer.
// In quickstream the length of the ring buffers depends on the read and
// write promises, and whither or not the buffer is a pass-through buffer
// or not, as in ringBuffers.c.

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

struct PassThroughPair {

    // The input and output are in the same block.  and they share the
    // same ring buffer.  Hence, the data is not copied to another buffer
    // when passing through this block; it's passing through the block
    // just by moving read and write pointers.
    struct QsInput *input;
    struct QsOutput *output;
};


static inline struct PassThroughPair
IsPassThrough(struct QsSimpleBlock *b, struct QsOutput *o) {

    struct PassThroughPair p;
    p.input = 0;
    p.output = 0;

    // Search this block's inputs and outputs for pass-through port
    // numbers.
    for(uint32_t i = b->numInputs - 1; i != -1; --i)
        for(uint32_t j = b->numOutputs - 1; j != -1; --j) {
            struct QsOutput *output = b->outputs + j;
            if(output != o) continue;
            for(uint32_t k = b->numPassThroughs - 1; k != -1; --k) {
                struct QsPassThrough *pt = b->passThroughs + k;
                if(pt->inputPortNum == i && pt->outputPortNum == j) {
                    p.input = b->inputs + i;
                    p.output = b->outputs + j;
                    return p;
                }
            }
        }

    // We pass a copy of a struct PassThroughPair back.
    return p;
}


static
void GetBuffer(struct QsBuffer *buffer,
        struct QsSimpleBlock *b,
        size_t mapLength, size_t overhangLength,
        // this output is an output in the block "b"
        struct QsOutput *output) {

    DASSERT(output->numInputs);
    DASSERT(output->inputs);

    if(!buffer) {
        buffer = calloc(1, sizeof(*buffer));
        ASSERT(buffer, "calloc(1,%zu) failed", sizeof(*buffer));
        output->buffer = buffer;
    }

    size_t maxRead = 0;

    for(uint32_t i = output->numInputs - 1; i != -1; --i) {
        if(maxRead < output->inputs[i]->maxRead)
            maxRead = output->inputs[i]->maxRead;
        output->inputs[i]->buffer = buffer;
    }
    // Add a reader length to the buffer mapping sizes.
    mapLength += maxRead;
    if(overhangLength < maxRead)
        overhangLength = maxRead;

    // TODO: If we allowed loops in the stream we'd have to change this
    // function.

    // There can only be at most one input that has pass-through write
    // access to a pass-through buffer per block output-to-inputs
    // connection stage.  There can be any number of connection
    // pass-through buffer stages.  So we keep going, calling GetBuffer()
    // and adding the needed buffer lengths via recursion.
    struct PassThroughPair p = IsPassThrough(b, output);
    if(p.input) {
        // We recurse.  This "input" is passing to a block downstream.
        DASSERT(p.input->block != b);
        DASSERT(p.input->feederBlock == b);
        // Add a writer length to the buffer mapping sizes.
        mapLength += p.output->maxWrite;
        if(overhangLength < p.output->maxWrite)
            overhangLength = p.output->maxWrite;

        // We will be passing data through this buffer in the block
        // that contains p.input and p.output.
        output->next = p.output;
        p.output->prev = output;
        p.output->buffer = buffer;
        p.input->buffer = buffer;

        GetBuffer(buffer, p.input->block, mapLength, 
                overhangLength, p.output);

        // We popped up of the recurring call that made a mapping at some
        // point.  Now we can get the initialize read and write pointers
        // to the ring buffer.

        p.input->readPtr = p.output->writePtr =
            buffer->end - buffer->mapLength;

        // This is the next output in which we need to do these crap for
        // it's inputs that have no ring buffers yet.
        output = p.output;

    } else {

        // This buffer does not pass-through this block "b" and its'
        // output.  This block, "b", and output, is at the end of a
        // "pass-through" buffer chain, and we have added all the buffer
        // length numbers from all the outputs and inputs that share this
        // buffer; so we can make the ring buffer memory mappings.
        buffer->mapLength = mapLength;
        buffer->overhangLength = overhangLength;

        buffer->end =
            makeRingBuffer(&buffer->mapLength, &buffer->overhangLength) +
            buffer->mapLength;
    }

    // Now that we have a mapping, i.e. the ring buffer addresses, so we
    // finish initializing inputs' readPtr data that this output feeds. 
    //
    // We assume that there are no loops in the block stream graph,
    // otherwise this will blow the function call stack.
    //
    // Also go to the next blocks that this output is feeding and make
    // those buffers if they are not already made.
    //
    for(uint32_t i = output->numInputs - 1; i != -1; --i) {
        struct QsInput *input = output->inputs[i];
        input->readPtr = buffer->end - buffer->mapLength;
        struct QsSimpleBlock *smB = input->block;
        for(uint32_t j = smB->numOutputs - 1; j != -1; --j) {
            struct QsOutput *o = smB->outputs + j;
            // We may have been to this block, smB, before via a different
            // input to this block, smB.  Hence if ... if is good.
            // https://www.youtube.com/watch?v=nqerQZObl58
            // Hercules.
            if(!o->buffer)
                // We keep going down the stream in all paths.  This will
                // get all buffers.  Note: we are using/building the
                // function call stack to traverse to each leaf (block
                // sink) in the flow graph.   There are no loops, so
                // this will terminate at a sink block.
                GetBuffer(0, smB, o->maxWrite, o->maxWrite, o);
        }
    }
}


void CreateRingBuffers(struct QsGraph *g) {

    DASSERT(g);
    DASSERT(g->sources);

    struct QsSimpleBlock **s = g->sources;

    for(struct QsSimpleBlock *b = *s; b; b = *(++s)) {
        DASSERT(b->outputs);
        DASSERT(b->numOutputs);
        for(uint32_t i = b->numOutputs - 1; i != -1; --i) {
            struct QsOutput *output = b->outputs + i;
            size_t maxWrite = output->maxWrite;
            GetBuffer(0, b, maxWrite, maxWrite, output);
        }
    }
}


void DestroyRingBuffers(struct QsGraph *g) {

    DASSERT(g);
    DASSERT(g->sources);

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        for(uint32_t i = smB->numInputs - 1; i != -1; --i) {
            struct QsInput *input = smB->inputs + i;
            input->readPtr = 0;
            input->readLength = 0;
            input->buffer = 0;
        }
        for(uint32_t i = smB->numOutputs - 1; i != -1; --i) {
            struct QsOutput *output = smB->outputs + i;
            output->writePtr = 0;
            output->next = 0;
            if(output->prev == 0) {
                // We let this output own the buffer.
                struct QsBuffer *buffer = output->buffer;
                DASSERT(buffer);
#ifdef DEBUG
                memset(buffer->end - buffer->mapLength, 0,
                    buffer->mapLength);
#endif
                freeRingBuffer(buffer->end - buffer->mapLength,
                        buffer->mapLength, buffer->overhangLength);
#ifdef DEBUG
                memset(buffer, 0, sizeof(*buffer));
#endif
                free(buffer);
            } else
                output->prev = 0;

            output->buffer = 0;
        }
    }
}


// The block's outputs need pointers to the inputs that they feed.
//
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


// Since the connections from outputs to inputs can change when the stream
// is paused; we need this function to free the output's input list.
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
