#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"



#define MAX_INPUTS  (9)


static const size_t maxWrite = 102;


int declare(void) {

    qsSetNumInputs(0, MAX_INPUTS);
    qsSetNumOutputs(0, MAX_INPUTS);

    for(uint32_t i=0; i<MAX_INPUTS; ++i)
        qsMakePassThroughBuffer(i, i);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    ASSERT(numInputs == numOutputs);

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    for(uint32_t i=0; i<numInputs; ++i) {
        qsSetOutputMax(i, maxWrite);
        qsSetInputMax(i, maxWrite);
    }

    return 0; // success
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void * const userData) {

    for(uint32_t i=0; i<numIn; ++i) {

        size_t len = inLens[i];
        if(len > outLens[i])
            len = outLens[i];
        if(len == 0)
            continue;

        // Advance both the reader and the writer.
        //
        qsAdvanceInput(i, len);
        qsAdvanceOutput(i, len);
    }

    return 0;
}
