#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#include "Sequence.h"

#define DEFAULT_SEEDOFFSET ((size_t) 0)


static size_t maxWrite = 0;
static char **compare;

static struct RandomString *rs;

static size_t seedOffset = DEFAULT_SEEDOFFSET;


int declare(void) {

    qsParameterConstantCreate(0, "seedOffset",
            QsSize, sizeof(seedOffset), 0/*setCallback*/,
            0/*userData*/, &seedOffset);
    qsBlockSetNumInputs(1,10);
    qsBlockSetNumOutputs(0,10);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    ASSERT(numInputs, "block %s", qsBlockGetName(0));

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));
    qsParameterGetValueByName("seedOffset", &seedOffset, sizeof(seedOffset));


    compare = calloc(numInputs, sizeof(*compare));
    ASSERT(compare, "calloc(%" PRIu32 ",%zu) failed",
            numInputs, sizeof(*compare));


    for(uint32_t i=0; i<numInputs; ++i)
        if(i >= numOutputs) {
            // Pass through buffers need another buffer to compare the
            // sequence to, and so do inputs that have no output.
            compare[i] = calloc(1, maxWrite + 1);
            ASSERT(compare[i], "calloc(1,%zu) failed", maxWrite + 1);
        }

    rs = calloc(numInputs, sizeof(*rs));
    ASSERT(rs, "calloc(%" PRIu32 ",%zu) failed",
            numInputs, sizeof(*rs));

    uint32_t seeds[numInputs];
    for(uint32_t i=0; i<numInputs; ++i)
        seeds[i] = i + seedOffset;

    for(uint32_t i=0; i<numInputs; ++i) {
        DSPEW("Block \"%s\" Input port %" PRIu32 " seed=%" PRIu32,
                qsBlockGetName(0), i, seeds[i]);
        // Initialize the random string generator.
        randomString_init(rs + i, seeds[i]);
    }

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

        char *in = buffers[i];
        char *out;
        char *comp;

        if(i < numOutputs)
            comp = out = qsGetOutputBuffer(i);
        else {
            DASSERT(compare[i]);
            // This has no corresponding output for this input port
            // so we use a buffer that we allocated in start().
            comp = compare[i];
        }

        randomString_get(rs + i, len, comp);

        for(size_t j=0; j<len; ++j)
            // Check each character.  We like to know where it fails.
            ASSERT(comp[j] == in[j],
                    "block %s Miss-match on input channel %" PRIu32,
                    qsBlockGetName(0), i);

        if(i < numOutputs)
            qsOutput(i, len);
    }

    if(numOutputs > numInputs)
        // We have excess outputs.
        for(uint32_t i=numOutputs-numInputs; i<numOutputs; ++i) {

            // Write the last input to the rest of the outputs.
            size_t len = lens[numInputs-1];
            if(len > maxWrite)
                len = maxWrite;
            else if(len == 0)
                continue;
            char *out = qsGetOutputBuffer(i);
            memcpy(out, buffers[numInputs-1], len);
            // This data that we are outputting was checked already.
            qsOutput(i, len);
        }

    return 0;
}


int stop(uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(rs);
    free(rs);
    rs = 0;

    DASSERT(compare);
 
    for(uint32_t i=0; i<numInputs; ++i)
        if(compare[i]) {
            free(compare[i]);
            compare[i] = 0;
        }

    free(compare);
    compare = 0;

    return 0;
}
