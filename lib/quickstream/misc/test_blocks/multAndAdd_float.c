#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include <sched.h>

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"


// Same as maxRead.
static size_t maxWrite = 1024;


// Out = In * a + b

static const float a = 3.182;
static const float b = 6.132;

static bool cpuSet;


int declare(void) {

    qsSetNumInputs(1, 1);
    qsSetNumOutputs(1, 1);
    qsMakePassThroughBuffer(0, 0);
    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    cpuSet = false;
    qsSetOutputMax(0, maxWrite);
    qsSetInputMax(0, maxWrite);

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

    if(false && !cpuSet) SetCPU(6);

    size_t len = inLens[0];
    if(len > outLens[0])
        len = outLens[0];
    if(len < sizeof(float))
        return 0;

    len -= len % sizeof(float);

    uint32_t numFloats = len / sizeof(float);

    float *x = out[0];
    float *end = x + numFloats;

    for(; x < end; ++x) {
        *x *= a;
        *x += b;
    }

    qsAdvanceInput(0, len);
    qsAdvanceOutput(0, len);

    return 0;
}
