// This is used in test ../../../../tests/233_simpleGetToSetControllers
// and ../../../../tests/243_simpleGetToSetNControllers
//
// Editing this file will likely break these tests.

#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static
struct QsParameter *constant = 0;


static
double val = 0.034;

#if 0
int stopItimer(void) {

    // This block needs a stop() function to stop and remove the periodic
    // signal generator.
    if(setitimer(ITIMER_REAL, 0, 0)) {
        ERROR("setitimer(ITIMER_REAL,0,0) failed");
        return -1; // fail
    }
    return 0; // success
}


int startItimer(void) {

    // This block needs a start() function to setup and start the periodic
    // signal generator.
    errno = 0;
    struct itimerval it = {
        { 0/*seconds*/, 1000/*microseconds*/ }, { 0, 1000 }};
        //{ 1/*seconds*/, 0/*microseconds*/ }, { 1,0 }};
    if(setitimer(ITIMER_REAL, &it, 0)) {
        ERROR("setitimer(ITIMER_REAL,,) failed");
        return -1; // fail
    }
    return 0;
}
#endif

static
int setCallback(struct QsParameter *p, const double *value, void *userData) {

    DSPEW("value=%lg", *value);
    return 0;
}


int declare(void) {

    constant = qsParameterConstantCreate(0, "constant", QsDouble,
            sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback,
            0, &val);

    DASSERT(constant);


    return 0; // 0 => success
}


int start(uint32_t numInPorts, uint32_t numOutPorts) {


    return 0; // success
}


int stop(uint32_t numInPorts, uint32_t numOutPorts) {



    return 0;
}
