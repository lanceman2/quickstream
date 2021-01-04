#include <stdbool.h>
#include <pthread.h>

#include "../include/quickstream/block.h" // public interfaces

#include "debug.h"
#include "block.h"
#include "graph.h"
#include "GET_BLOCK.h"



#define GET_SIMPLEBLOCK_IN_FLOW(b) \
    do {\
        b = (struct QsSimpleBlock *) GetBlock();\
        ASSERT(((struct QsBlock *)b)->isSuperBlock == 0);\
        ASSERT(((struct QsBlock *)b)->inWhichCallback == _QS_IN_FLOW,\
                "%s() must be called from flow() or flush()", __func__);\
    } while(0)



void *qsGetOutputBuffer(uint32_t outputPortNum) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_FLOW(b);

    ASSERT(b->numOutputs > outputPortNum,
            "Block \"%s\" only has %" PRIu32 " output ports"
            ", but block requested port number %" PRIu32,
            b->block.name,
            b->numOutputs, outputPortNum);

    DASSERT(b->outputs);
    DASSERT((b->outputs + outputPortNum)->numInputs);

    return (b->outputs + outputPortNum)->writePtr;
}


void qsOutput(uint32_t outputPortNum, size_t len) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_FLOW(b);

    ASSERT(b->numOutputs > outputPortNum,
            "Block \"%s\" only has %" PRIu32 " output ports",
            b->block.name, b->numOutputs);
    struct QsOutput *output = b->outputs + outputPortNum;
    DASSERT(len <= output->maxWrite);

    // They can call this more than once in a given flow() call, so they
    // can output a little at a time, in a convoluted code; for
    // convenience.

    // This is all this function needed to do.  We do not advance the
    // write pointer yet.
    b->outputLens[outputPortNum] += len;

    // The OUTPUT WRITE PROMISE ERROR CHECK
    //
    // Check for this block writer error:
    ASSERT(b->outputLens[outputPortNum] <= output->maxWrite,
                "block \"%s\" writing %zu which is greater"
                " than the %zu promised",
                b->block.name, b->outputLens[outputPortNum],
                output->maxWrite);
}


void qsAdvanceInput(uint32_t inputPortNum, size_t len) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_FLOW(b);

    ASSERT(inputPortNum < b->numInputs);

    b->advanceLens[inputPortNum] += len;

    DASSERT(b->inputs);

    // CHECKING READ OVERRUN
    //
    // Check if the buffer is being over-read.  If the filter really read
    // this much data than it will have read past the write pointer.
    ASSERT(b->advanceLens[inputPortNum] <=
            b->inputs[inputPortNum].readLength,
            "Block \"%s\" on input port %" PRIu32
            " tried to read to much %zu > %zu available",
            b->block.name,
            inputPortNum, b->advanceLens[inputPortNum],
            b->inputs[inputPortNum].readLength);
}
