#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#include "Sequence.h"

#define DEFAULT_SEEDOFFSET ((uint32_t) 0)


static size_t maxWrite;
static char **compare;
static struct RandomString *rs;
static const char *blockName;
static uint32_t seedOffset = DEFAULT_SEEDOFFSET;
static const char *seedsString = 0;
static const char *passThroughList = 0;


int declare(void) {

    return 0; // success
}


int construct(void) {

    DSPEW();

    maxWrite = QS_DEFAULTMAXWRITE;

    seedOffset = seedOffset;

    seedsString = 0;

    passThroughList = 0;

    ASSERT(maxWrite);

    blockName = qsGetBlockName();

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    ASSERT(numInputs);

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    compare = calloc(numInputs, sizeof(*compare));
    ASSERT(compare, "calloc(%" PRIu32 ",%zu) failed",
            numInputs, sizeof(*compare));


    uint32_t passThrough[numInputs];
    for(uint32_t i=0; i<numInputs; ++i)
        passThrough[i] = -1;

    if(passThroughList) {
        unsigned int inPort, outPort;
        const char *str = passThroughList;
        while(*str && sscanf(str, "%u", &inPort) == 1) {
            // Go to the next number in the string str.
            while(*str && (*str >= '0' && *str <= '9')) ++str;
            while(*str && (*str < '0' || *str > '9')) ++str;
            if(*str && sscanf(str, "%u", &outPort) == 1) {
                if(inPort < numInputs && outPort < numOutputs)
                    passThrough[inPort] = outPort;
                // Go to the next number in the string str.
                while(*str && (*str >= '0' && *str <= '9')) ++str;
                while(*str && (*str < '0' || *str > '9')) ++str;
            }
        }
    }

    for(uint32_t i=0; i<numInputs; ++i)
        if(passThrough[i] != -1 || i >= numOutputs) {
            // Pass through buffers need another buffer to compare the
            // sequence to, and so do inputs that have no output.
            compare[i] = calloc(1, maxWrite + 1);
            ASSERT(compare[i], "calloc(1,%zu) failed", maxWrite + 1);
        }

    for(uint32_t i=0; i<numOutputs; ++i) {
        if(passThrough[i] == -1)
            qsCreateOutputBuffer(i, maxWrite);
        else
            if(qsCreatePassThroughBuffer(i, passThrough[i], maxWrite)) {
                // TODO: We could return an error and fail the whole run.
                //
                // We fall back to making it a regular output.
                passThrough[i] = -1;
                qsCreateOutputBuffer(i, maxWrite);
            }
    }

    rs = calloc(numInputs, sizeof(*rs));
    ASSERT(rs, "calloc(%" PRIu32 ",%zu) failed",
            numInputs, sizeof(*rs));

    uint32_t seeds[numInputs];
    for(uint32_t i=0; i<numInputs; ++i)
        seeds[i] = i + seedOffset;

    if(seedsString) {
        unsigned int val;
        const char *str = seedsString;
        uint32_t i = 0;
        while(i < numInputs && *str && sscanf(str, "%u", &val) == 1) {
            seeds[i++] = val;
            // Go to the next number in the string str.
            while(*str && (*str >= '0' && *str <= '9')) ++str;
            while(*str && (*str < '0' || *str > '9')) ++str;
        }
    }

    for(uint32_t i=0; i<numInputs; ++i) {
        DSPEW("Filter \"%s\" Input port %" PRIu32 " seed=%" PRIu32,
                blockName, i, seeds[i]);
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
                    "%s Miss-match on input channel %" PRIu32,
                    blockName, i);

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
