#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"
#include "../../../../lib/parseBool.h"


#include "Sequence.h"



static size_t maxWrite = 512;

static size_t totalOut[MAX_OUTPUTS]; // amount we write per cycle
static size_t count[MAX_OUTPUTS];    // current amount so far

static
uint32_t seeds[MAX_OUTPUTS];

static
bool finished[MAX_OUTPUTS];


static struct RandomString *rs;


static
char *SetSeeds(int argc, const char * const *argv, void *userData) {


    qsParseUint32tArray(seeds[0], seeds, MAX_OUTPUTS);

    for(int j=0; j<MAX_OUTPUTS; ++j)
        fprintf(stderr, "seeds[%d]=%" PRIu32 "\n", j, seeds[j]);

    return 0;
}


static
bool syncOutput = false;

static
char *SyncOutput(int argc, const char * const *argv, void *userData) {


    if(argc < 2)
        syncOutput = true;
    else
        syncOutput = qsParseBool(argv[1]);

INFO("syncOutput=%d", syncOutput);

    return 0;
}


static
char *SetOutputLengths(int argc, const char * const *argv, void *userData) {

    qsParseSizetArray(totalOut[0], totalOut, MAX_OUTPUTS);

    for(int j=0; j<MAX_OUTPUTS; ++j)
        fprintf(stderr, "totalOut[%d]=%zu\n", j, totalOut[j]);

    return 0;
}



int declare(void) {

    qsSetNumInputs(0,  0);
    qsSetNumOutputs(1, MAX_OUTPUTS);

    qsAddConfig(SyncOutput, "SyncOutput",
            "Cause all output ports to write the same number of"
            " bytes at a time.",
            "SyncOutput [BOOL]",
            "default args");

    qsAddConfig(SetOutputLengths, "TotalOutputBytes",
            "Total bytes written per cycle."
            "  0 is for infinite output. "
            "List of values for each output port, "
            "with the last value for the rest of "
            "the ports not listed.",
            "TotalOutputBytes BYTES",
            "TotalOutputBytes whatever");

    qsAddConfig(SetSeeds, "Seeds",
            "Generator seed per port per cycle."
            "List of values for each output port, "
            "with the last value for the rest of "
            "the ports not listed. ",
            "Seeds NUM",
            "Seeds whatever");


    for(uint32_t i=0; i<MAX_OUTPUTS; ++i) {
        // Set defaults
        totalOut[i] = DEFAULT_TOTAL_LENGTH;
        seeds[i] = DEFAULT_SEED_OFFSET + i;
    }


    return 0; // success
}


static size_t c0;

int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    // This is a source filter block
    DASSERT(numInputs == 0);
    DASSERT(numOutputs >= 1);
    DASSERT(numOutputs <= MAX_OUTPUTS);

    DSPEW("Block \"%s\" %" PRIu32 " outputs",
            qsBlockGetName(), numOutputs);


    rs = calloc(numOutputs, sizeof(*rs));
    ASSERT(rs, "calloc(%" PRIu32 ",%zu) failed",
            numOutputs, sizeof(*rs));

    for(uint32_t i=0; i<numOutputs; ++i) {
        // Initialize the random string generator.
        randomString_init(rs + i, seeds[i]);
        qsSetOutputMax(i, maxWrite);
        count[i] = 0;
        finished[i] = false;
    }

    c0 = 0;

    return 0; // success
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {


    if(syncOutput) {

        size_t len = maxWrite;

        for(uint32_t i=0; i<numOut; ++i)
            if(len > outLens[i])
                len = outLens[i];

        if(!len) return 0;

        if(finished[0]) return 1; // done.


        if(totalOut[0] && count[0] + len >= totalOut[0]) {
            len = totalOut[0] - count[0];
            finished[0] = true;
        }

        for(uint32_t i=0; i<numOut; ++i) {
            count[i] += len;
            randomString_get(rs + i, len, out[i]);
            qsAdvanceOutput(i, len);
            if(finished[0])
                WARN("Block \"%s\" Output port %" PRIu32
                        " finished %zu bytes",
                        qsBlockGetName(), i, totalOut[i]);
        }


        if(finished[0]) return 1; // done.
        return 0; // keep going.
    }


    int ret = 1; // Start by being marked as done.


    for(uint32_t i=0; i<numOut; ++i) {

        // lenOut (outLens[i]) will be maxWrite or less.

        size_t lenOut = outLens[i];

        DASSERT(lenOut <= maxWrite);
        
        if(finished[i]) continue;

        if(!lenOut) {
            ret = 0;
            continue;
        }

        // Tally the count so far for this channel, i.
        if(totalOut[i] && (count[i] + lenOut) >= totalOut[i]) {
            lenOut = totalOut[i] - count[i];
            WARN("Block \"%s\" output port %" PRIu32
                    " sent final %zu bytes",
                    qsBlockGetName(), i, totalOut[i]);
            finished[i] = true;
            qsOutputDone(i);
        } else
            ret = 0; // This channel, i, is not done.


        count[i] += lenOut;

        randomString_get(rs + i, lenOut, out[i]);
        qsAdvanceOutput(i, lenOut);
    }

    if(ret)
        INFO("FINISHED");


    // If ret = 0 we have at least one output port being written.

    return ret;
}


int stop(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    DASSERT(numInputs == 0);
    DASSERT(numOutputs);
    DASSERT(rs);

    free(rs);
    rs = 0;

    return 0;
}
