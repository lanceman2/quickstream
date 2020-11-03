#include "../../../../../include/quickstream/block.h"
#include "../../../../../lib/debug.h"

#include "Sequence.h"

// This is the default total output length to all output channels
// for a given stream run.
#define DEFAULT_TOTAL_LENGTH   ((size_t) 800000)


static size_t maxWrite;
static size_t totalOut, count;
static struct RandomString *rs;


int construct(void) {

    DSPEW();

    maxWrite = QS_DEFAULTMAXWRITE;
    totalOut = DEFAULT_TOTAL_LENGTH;

    ASSERT(maxWrite);
    ASSERT(totalOut);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    ASSERT(numInputs == 0);
    ASSERT(numOutputs);

    DSPEW("%" PRIu32 " outputs", numOutputs);

    rs = calloc(numOutputs, sizeof(*rs));
    ASSERT(rs, "calloc(%" PRIu32 ",%zu) failed",
            numOutputs, sizeof(*rs));

    for(uint32_t i=0; i<numOutputs; ++i) {
        qsCreateOutputBuffer(i, maxWrite);
        // Initialize the random string generator.
        randomString_init(rs + i, i/*seed*/);
    }


    count = 0;

    return 0; // success
}


int work(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(numInputs == 0);
    DASSERT(numOutputs);

    int ret = 0;

    size_t len = maxWrite;
    if(count + len <= totalOut)
        count += len;
    else {
        len = totalOut - count;
        ret = 1; // Last time calling input().
    }

    if(len == 0) return 1; // We're done.

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
