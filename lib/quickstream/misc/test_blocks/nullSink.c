#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include <sched.h>
#include <time.h>

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"


static size_t maxRead = 1024;
static bool cpuSet;
static size_t count;
struct timespec t0;

int declare(void) {

    qsSetNumInputs(1, 1);
    qsSetNumOutputs(0, 0);
    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    count = 0;
    cpuSet = false;
    ASSERT(0 == clock_gettime(CLOCK_REALTIME, &t0));
    qsSetInputMax(0, maxRead);
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
    if(!len) return 0;
    count += len;
    qsAdvanceInput(0, len);
    return 0;
}


int stop(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    struct timespec t1;
    ASSERT(0 == clock_gettime(CLOCK_REALTIME, &t1));

    double time = (t1.tv_sec - t0.tv_sec)  +  1.0e-9 *t1.tv_nsec -  1.0e-9 * t0.tv_nsec;

    fprintf(stderr, "Read %zu bytes = %zu floats -> %lg floats/second\n",
            count, count/sizeof(float), (count/sizeof(float))/time);
    return 0;
}
