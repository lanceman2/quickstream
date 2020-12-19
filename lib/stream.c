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


    struct QsGraph *g = _from->graph;
    DASSERT(g);
    DASSERT(_to->graph == g);
    if(_to->graph != g || !g) {
        ERROR("Blocks are not from the same graph");
        return -3; // error
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
        return -4; // error
    }
    // It's okay if an output port has a connection already.

    // Make sure that from->outputs has the needed number of ports.
    // Port numbers start at 0, just like an array index that it is.
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
    }

    // Make sure that to->inputs has the needed number of ports.
    // Port numbers start at 0, just like an array index that it is.
    if(to->numInputs <= toPortNum) {
        // We need to add an input port.  We'll check that all input
        // ports in the array get used before we start the streams.
        uint32_t oldNum = to->numInputs;
        to->numInputs = toPortNum + 1;
        to->inputs = realloc(to->inputs,
                to->numInputs*sizeof(*to->inputs));
        ASSERT(to->inputs, "realloc(,%zu) failed",
                to->numInputs*sizeof(*to->inputs));
        // Realloc() does not initialize, so we do here, just the ones
        // that we just added.  Any old memory is saved by realloc().
        memset(to->inputs + oldNum, 0,
                (to->numInputs - oldNum)*sizeof(*to->inputs));
    }

    // Now connect the blocks.

    struct QsOutput *output = from->outputs + fromPortNum;
    struct QsInput *input = to->inputs + toPortNum;
    // Add an input to the output, ha ha.
    //
    // Each output port keeps a list of the to block inputs.
    ++output->numInputs;
    output->inputs = realloc(output->inputs, output->numInputs*
            sizeof(*output->inputs));
    ASSERT(output->inputs, "realloc(,%zu) failed",
            output->numInputs*sizeof(*output->inputs));
    output->inputs[output->numInputs - 1] = input;

    input->block = to;
    input->feederBlock = from;
    input->inputPortNum = toPortNum;
    input->outputPortNum = fromPortNum;
 
    return 0; // success
}


// Returns true if the output no longer has inputs connected.
static inline
bool RemoveInputFromOutput(struct QsBlock *from, struct QsBlock *to,
        struct QsOutput *output, struct QsInput *input,
        uint32_t outputPortNum, uint32_t inputPortNum) {

    DASSERT(output->buffer == 0);

    // Remove this input from the output inputs array of pointers.
    uint32_t i = 0;
    for(; i < output->numInputs; ++i)
        if(*(output->inputs + i) == input)
            // We found the input that came from this output.
            break;
    // We're fucked if we did not find it.  It's a code error.
    DASSERT(i < output->numInputs,
            "Cannot find input from output from "
            "block \"%s\" port %" PRIu32 " to block %s port %"
            PRIu32, from->name, outputPortNum,
            to->name, inputPortNum);
    // i is now the index of this input
    //
    // starting at that index + 1
    for(++i; i < output->numInputs; ++i)
        // shift the rest of the array back one index
        output->inputs[i-1] = output->inputs[i];
    // This will be the new length:
    --output->numInputs;
#ifdef DEBUG
    // Zero out just the memory that will be freed.  Just one entry.
    memset(output->inputs + output->numInputs, 0,
            sizeof(*output->inputs));
#endif
    if(output->numInputs) {
        // Shrink the memory.
        output->inputs = realloc(output->inputs,
                output->numInputs*sizeof(*output->inputs));
        ASSERT(output->inputs, "realloc(,%zu) failed",
                output->numInputs*sizeof(*output->inputs));
    } else {
        free(output->inputs);
        output->inputs = 0;
    }

    if(output->inputs == 0) {
        // This output is nulled out now.
        memset(output, 0, sizeof(*output));
        // No inputs connected.
        return true;
    }
    // We still have some inputs connected.
    return false;
}


// We only need the block with the input port to remove a connection,
// because each input port can only have one connection.
//
int qsBlockDisconnect(struct QsBlock *b, uint32_t inputPortNum) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    DASSERT(b);
    DASSERT(b->graph);
    DASSERT(!b->isSuperBlock);

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

    // There should be no ring buffer at this time.  Random thing to
    // check, WTF dude.
    DASSERT((from->outputs + outputPortNum)->buffer == 0);


    // 1.  Remove the "from" block output data for port outputPortNum

    if(RemoveInputFromOutput(&from->block, &to->block,
                from->outputs + outputPortNum,
                to->inputs + inputPortNum, outputPortNum, inputPortNum)) {
        // We no longer have any inputs connected to this output.

        if(outputPortNum == from->numOutputs - 1) {
            // The output port number is the last one in the "from" block
            // outputs[] array. We squeeze and outputs array down in size,
            // down to the largest index in the array that has a
            // connection.


#ifdef DEBUG
            memset(from->outputs + outputPortNum, 0, sizeof(*from->outputs));
#endif
            // free this end of the output array
            --from->numOutputs;

            if(from->numOutputs) {

                from->outputs = realloc(from->outputs,
                        from->numOutputs*sizeof(*from->outputs));
                ASSERT(from->outputs, "realloc(,%zu) failed",
                         from->numOutputs*sizeof(*from->outputs));
            } else {
                free(from->outputs);
                from->outputs = 0;
            }
        } else {
            // This output port is not at the end of the array.
            // We zero out the entry.
            memset(from->outputs + outputPortNum, 0, sizeof(*from->outputs));
        }
    }


    // 2. Remove "to" input data from port inputPortNum

    if(inputPortNum == to->numInputs - 1) {
        // The input port number is the last one to "to" block, so we can
        // squeeze and inputs array down in size, down to the largest
        // index in the array that has a connection (feederBlock).
        // inputs[] without a connection will have feederBlock being 0.
        uint32_t inputPortMax = to->numInputs - 2;
        for(; inputPortMax != -1; --inputPortMax)
            if(to->inputs[inputPortMax].feederBlock)
                break;
        // The new size of the inputs[] array will be:
        to->numInputs = inputPortMax + 1;
#ifdef DEBUG
        // zero out all the memory that we will free:
        memset(to->inputs + to->numInputs, 0,
                (inputPortNum - inputPortMax)*sizeof(*to->inputs));
#endif
        if(to->numInputs) {
            // Shrink the memory.
            to->inputs = realloc(to->inputs,
                    to->numInputs*sizeof(*to->inputs));
            ASSERT(to->inputs, "realloc(,%zu) failed",
                    to->numInputs*sizeof(*to->inputs));
        } else {
            free(to->inputs);
        }
    } else {
        // We need to zero out the connection data in the inputs[] array
        // element.
        // This is not just in the DEBUG case.   We need to know that this
        // port number is a null connection by filling it with zeros.
        //
        // This will leave a gap in the array.  It's up to the user to
        // fix that.  We just know it's a gap, and mark it as such.
        memset(to->inputs + inputPortNum, 0, sizeof(*to->inputs));
    }

    return 0;
}




// Allocate stream ring buffers and ...
int StreamsInit(struct QsGraph *g) {

    DASSERT(g);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");


    // Check the stream graph has no loops.


    // Build the stream graph.
    



    return 0; // success
}


// Free the stream ring buffers and ...
void StreamFree(struct QsGraph *g) {



}
