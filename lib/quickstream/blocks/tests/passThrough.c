#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#define DEFAULT_SEEDOFFSET ((size_t) 0)

// Maximum number of input, and outputs given that the number of input is
// equal to the number of outputs.
#define MAX_INPUTS  (5)

static size_t maxWrite = 0;


int declare(void) {

    for(uint32_t i=0; i<MAX_INPUTS; ++i)
        qsPassThroughBuffer(i, i);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    ASSERT(numInputs <= MAX_INPUTS);
    ASSERT(numInputs);
    ASSERT(numInputs == numOutputs);

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));

    return 0; // success
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    for(uint32_t i=0; i<numInputs; ++i) {

        size_t len = lens[i];
        if(len > maxWrite)
            len = maxWrite;
        else if(len == 0)
            continue;

        qsAdvanceInput(i, len);
        qsOutput(i, len);
    }

    return 0;
}
