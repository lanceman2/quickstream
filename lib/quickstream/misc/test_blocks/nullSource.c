#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include <sched.h>

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"
#include "../../../../lib/parseBool.h"



static size_t maxWrite = 1024;
static size_t totalOut; // amount we write per cycle
static size_t count;    // current amount so far
static bool cpuSet;

static
char *SetOutputLengths(int argc, const char * const *argv,
        void *userData) {

    qsParseSizetArray(totalOut, &totalOut, 1);
    fprintf(stderr, "totalOut=%zu\n", totalOut);
    return 0;
}



int declare(void) {

    qsSetNumInputs(0,  0);
    qsSetNumOutputs(1, 1);

    qsAddConfig(SetOutputLengths, "TotalOutputBytes",
            "Total bytes written per cycle."
            "  0 is for infinite output. "
            "List of values for each output port, "
            "with the last value for the rest of "
            "the ports not listed.",
            "TotalOutputBytes BYTES",
            "TotalOutputBytes whatever");

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    cpuSet = false;

    DSPEW("Block \"%s\"", qsBlockGetName());
    qsSetOutputMax(0, maxWrite);
    count = 0;

    return 0; // success
}


static inline void SetCPU(int cpu) {

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    ASSERT(0 == sched_setaffinity(0/*0=>this thread*/, sizeof(set), &set));
    cpuSet = true;
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    size_t lenOut = outLens[0];

    if(false && !cpuSet) SetCPU(6);
    if(!lenOut) return 0;

    if(totalOut && (count + lenOut) < totalOut) {
        // Most used case here:
        count += lenOut;
        qsAdvanceOutput(0, lenOut);
        return 0;
    }

    //else (totalOut && (count + lenOut) >= totalOut) {
    lenOut = totalOut - count;
    count += lenOut;
    if(lenOut)
        qsAdvanceOutput(0, lenOut);
    INFO("Block \"%s\" sent final of %zu bytes",
                    qsBlockGetName(), count);
    qsOutputDone(0);
    return 1;
}
