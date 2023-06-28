#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"

#include "Sequence.h"



#define MAX_INPUTS  MAX_OUTPUTS


static size_t maxWrite = 1027;
static char **compare;

static struct RandomString *rs;
static uint32_t seeds[MAX_INPUTS];

static size_t count[MAX_INPUTS];    // current amount so far
static size_t totalOut[MAX_INPUTS]; // amount we must write per cycle


// Is input port to output port a "pass through" buffer.
//
// We only pass through from input port number to the same output port
// number.
//
static bool passThroughs[MAX_INPUTS];


static int retStatus = 0; // 0=success


// Examples:
//
// The default is like:
//   --configure-mk MK bname PassThrough false MK
//
// Other examples:
//   --configure-mk MK bname PassThrough false true false MK
//   --configure-mk MK bname PassThrough TrUE MK
//
static
char *SetPassThrough(int argc, const char * const *argv, void *userData) {

    qsParseBoolArray(true, passThroughs, MAX_INPUTS);

    for(uint32_t i=0; i<MAX_INPUTS; ++i)
        if(passThroughs[i])
            qsMakePassThroughBuffer(i, i);
        else
            qsUnmakePassThroughBuffer(i);

#if 1
    for(int32_t i=0; i<MAX_INPUTS; ++i)
        fprintf(stderr, "passThroughs[%" PRIu32 "]=%d\n",
                i, passThroughs[i]);
#endif

    return 0; // success
}


// Examples:
//
//   --configure-mk MK bname Seeds 33 44 45 55 67 MK
//   --configure-mk MK bname Seeds 2 MK
//
static
char *SetSeeds(int argc, const char * const *argv, void *userData) {

    qsParseUint32tArray(seeds[0], seeds, MAX_INPUTS);

#if 1
    for(int j=0; j<MAX_INPUTS; ++j)
        fprintf(stderr, "seeds[%d]=%" PRIu32 "\n", j, seeds[j]);
#endif

    return 0; // success
}


// examples:
//
//   --configure-mk bname OutputLengths 2134 4443 42355 MK
//   --configure-mk bname OutputLengths 1023 MK
//
static
char *SetOutputLengths(int argc, const char * const *argv, void *userData) {

    qsParseSizetArray(totalOut[0], totalOut, MAX_INPUTS);

#if 1
    for(int j=0; j<MAX_INPUTS; ++j)
        fprintf(stderr, "totalOut[%d]=%zu\n", j, totalOut[j]);
#endif

    return 0; // success
}



int declare(void) {

    qsSetNumInputs(0, MAX_INPUTS);
    qsSetNumOutputs(0, MAX_INPUTS);

    // Default configuration
    for(uint32_t i=0; i<MAX_INPUTS; ++i) {
        seeds[i] = DEFAULT_SEED_OFFSET + i;
        totalOut[i] = DEFAULT_TOTAL_LENGTH;
        passThroughs[i] = false;
    }

    qsAddConfig(SetOutputLengths, "TotalOutputBytes",
            "Check total bytes written per cycle."
            "  0 is for infinite output.  "
            "List of values for each output port, "
            "with the last value for the rest of "
            "the ports not listed.",
            "TotalOutputBytes BYTES",
            "TotalOutputBytes whatever");

    qsAddConfig(SetSeeds, "Seeds",
            "Generator seed per port per cycle."
            "List of values for each input port, "
            "with the last value for the rest of "
            "the ports not listed.",
            "Seeds NUM",
            "Seeds whatever");

    qsAddConfig(SetPassThrough, "PassThrough",
            "PassThrough IN_PORT ...",
            "PassThrough IN_PORT ...",
            "PassThrough whatever");

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    ASSERT(numInputs >= numOutputs);

    if(!numInputs) return 0;

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    compare = calloc(numInputs, sizeof(*compare));
    ASSERT(compare, "calloc(%" PRIu32 ",%zu) failed",
            numInputs, sizeof(*compare));

    rs = calloc(numInputs, sizeof(*rs));
    ASSERT(rs, "calloc(%" PRIu32 ",%zu) failed",
            numInputs, sizeof(*rs));


    for(uint32_t i=0; i<numInputs; ++i) {
        count[i] = 0;
        if(i < numOutputs)
            qsSetOutputMax(i, maxWrite);
        qsSetInputMax(i, maxWrite);
        compare[i] = calloc(1, maxWrite + 1);
        ASSERT(compare[i], "calloc(1,%zu) failed", maxWrite + 1);
        DSPEW("Input port %" PRIu32 " seed=%" PRIu32, i, seeds[i]);
        // Initialize the random string generator.
        randomString_init(rs + i, seeds[i]);
    }

    return 0; // success
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {


    for(uint32_t i=0; i<numIn; ++i) {

        size_t len = inLens[i];
        ASSERT(len <= maxWrite);

        if(numOut > i && len > outLens[i]) {
            DASSERT(outLens[i] <= maxWrite);
            len = outLens[i];
        }

        if(len == 0)
            continue;

        count[i] += len;

        qsAdvanceInput(i, len);

        // We could have used the output port for this for the
        // non-pass-through case, but it's just a test and so we
        // don't need to optimize it.
        randomString_get(rs + i, len, compare[i]);

        for(size_t j=0; j<len; ++j)
            // Check each character.  We like to know where it fails.
            ASSERT(compare[i][j] == ((char **)in)[i][j],
                    "block \"%s\" sequence miss-match on "
                    "input channel %" PRIu32 " at count=%zu"
                    " first count being 0",
                    qsBlockGetName(), i, count[i] - len + j);


        if(i < numOut) {

            // Advance the output write pointer:
            qsAdvanceOutput(i, len);

            if(!passThroughs[i])
                // Fill the output buffer for the non-"pass-through"
                // case:
                memcpy(out[i], in[i], len);
            // For the "pass through" case the output buffer is the same
            // as the input buffer.
        }

        if(totalOut[i] && count[i] == totalOut[i])
            DSPEW("Block \"%s\" counted %zu bytes on input port %"
                    PRIu32, qsBlockGetName(), count[i], i);
    }

    return 0;
}


int stop(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    if(!numInputs) return 0;


    for(uint32_t i=0; i<numInputs; ++i) {
        if(totalOut[i] && 
                !retStatus &&
                !(count[i] == 0 || totalOut[i] == count[i])) {
            retStatus = 1;
            // TODO: make this ASSERT() optional??
            // and get the return status at destroy().
            ASSERT(totalOut[i] == count[i],
                 "Block \"%s\" Input port %" PRIu32
                 ": Read %zu bytes, needed %zu bytes;"
                 " a difference of %zu",
                 qsBlockGetName(),
                 i, count[i], totalOut[i],
                 totalOut[i] - count[i]);
        }
        DSPEW("Block \"%s\" counted and checked %zu "
                "bytes on input port %" PRIu32,
                qsBlockGetName(), count[i], i);
    }

    DASSERT(rs);
    DASSERT(compare);

    free(rs);
    rs = 0;

    ASSERT(compare);
 
    for(uint32_t i=0; i<numInputs; ++i) {

        DASSERT(compare[i]);
#ifdef DEBUG
        memset(compare[i], 0, (maxWrite + 1)*sizeof(*compare[i]));
#endif
        free(compare[i]);
    }

#ifdef DEBUG
    memset(compare, 0, numInputs*sizeof(*compare));
#endif

    free(compare);
    compare = 0;

    return 0;
}


int destroy(void *userData) {

    return retStatus;
}
