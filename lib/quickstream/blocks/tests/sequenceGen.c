#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#include "Sequence.h"

// This is the default total output length to all output channels
// for a given stream run.
#define DEFAULT_TOTAL_LENGTH   ((size_t) 8000)


static size_t maxWrite = 0;
static size_t totalOut = DEFAULT_TOTAL_LENGTH, count;
static struct RandomString *rs;



int declare(void) {

    qsParameterConstantCreate(0, "totalOut",
            QsSize, sizeof(totalOut), 0/*setCallback*/,
            0/*userData*/, &totalOut);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    // This is a source filter block
    ASSERT(numInputs == 0);
    ASSERT(numOutputs);

    DSPEW("%" PRIu32 " outputs", numOutputs);


    rs = calloc(numOutputs, sizeof(*rs));
    ASSERT(rs, "calloc(%" PRIu32 ",%zu) failed",
            numOutputs, sizeof(*rs));

    for(uint32_t i=0; i<numOutputs; ++i) {
        // Initialize the random string generator.
        randomString_init(rs + i, i/*seed*/);
    }

    qsParameterGetValueByName("totalOut", &totalOut, sizeof(totalOut));
    if(totalOut == 0) totalOut = -1; // large amount
    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));


    // Bytes out so far in this run.
    count = 0;

    return 0; // success
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(numInputs == 0);
    DASSERT(numOutputs);

    int ret = 0;

    size_t len = maxWrite;
    if(count + len <= totalOut)
        count += len;
    else {
        len = totalOut - count;
        ret = 1; // Last time calling flow() for this run.
    }

    //if(len == 0) return 1; // We're done.

    for(uint32_t i=0; i<numOutputs; ++i) {
        void *out = qsGetOutputBuffer(i);
        randomString_get(rs + i, len, out);
        qsOutput(i, len);
    }

    return ret;
}


int stop(uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(numInputs == 0);
    DASSERT(numOutputs);
    DASSERT(rs);

    free(rs);
    rs = 0;

    return 0;
}
