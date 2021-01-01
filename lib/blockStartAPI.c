#include <pthread.h>

#include "../include/quickstream/block.h" // public interfaces

#include "debug.h"
#include "block.h"
#include "graph.h"


#define GET_SIMPLEBLOCK_IN_START(b) \
    do {\
        ASSERT(mainThread == pthread_self(), "Not graph main thread");\
        b = (struct QsSimpleBlock *) GetBlock();\
        ASSERT(((struct QsBlock *)b)->isSuperBlock == 0);\
        ASSERT(((struct QsBlock *)b)->inWhichCallback == _QS_IN_START,\
                "%s() must be called from flow() or flush()", __func__);\
    } while(0)




void qsSetInputThreshold(uint32_t inputPortNum, size_t len) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_START(b);

    if(inputPortNum < b->numInputs)
        b->inputs[inputPortNum].threshold = len;
}


void qsSetInputReadPromise(uint32_t inputPortNum, size_t len) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_START(b);

    if(inputPortNum < b->numInputs)
        b->inputs[inputPortNum].maxRead = len;
}


void qsSetOutputMaxWrite(uint32_t outputPortNum, size_t len) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_START(b);

    if(outputPortNum < b->numOutputs)
        b->outputs[outputPortNum].maxWrite = len;
}


size_t qsGetOutputMaxWrite(uint32_t outputPortNum) {

    struct QsSimpleBlock *b;
    GET_SIMPLEBLOCK_IN_START(b);

    if(outputPortNum < b->numOutputs)
        return b->outputs[outputPortNum].maxWrite;
    return 0;
}
