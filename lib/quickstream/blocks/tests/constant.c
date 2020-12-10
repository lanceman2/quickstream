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
struct QsParameter *getter = 0;

static int count = 0;


int stopItimer(void) {

    // This block needs a stop() function to stop and remove the periodic
    // signal generator.
    if(setitimer(ITIMER_REAL, 0, 0)) {
        ERROR("setitimer(ITIMER_REAL,0,0) failed");
        return -1; // fail
    }
    return 0; // success
}

static
double val = 0.034;



static
int triggerCallback(struct QsParameter *p) {

    DSPEW("                      TIMER %d", ++count);


    qsParameterGetterPush(getter, &val);

    val += 2.0;


    if(count > 9) {
        stopItimer();
        // Stop using the itimer
        return 1; // 1 = stop calling this callback
    }
    return 0;
}


int bootstrap(void) {


    getter = qsParameterGetterCreate(0, "getter", QS_DOUBLE, sizeof(val));
    DASSERT(getter);

    qsTriggerSignalCreate(SIGALRM, (int (*)(void *)) triggerCallback,
            /*userData*/getter);

    return 0; // 0 => success
}

int start(uint32_t numInPorts, uint32_t numOutPorts) {

    count = 0;

    if(qsParameterNumConnections(getter) == 0) return 0;

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

    return 0; // success
}

int stop(uint32_t numInPorts, uint32_t numOutPorts) {


    if(qsParameterNumConnections(getter) == 0) return 0;

    return stopItimer();
}
