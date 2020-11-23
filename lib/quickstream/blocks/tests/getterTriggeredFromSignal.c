#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"

static
struct QsParameter *getter = 0;


static
void triggerCallback(struct QsParameter *p) {

    DSPEW("              TIMER");
}


int bootstrap(void) {

    getter = qsParameterGetterCreate(0, "get", QS_DOUBLE, sizeof(double));
    DASSERT(getter);

    qsTriggerSignalCreate(SIGALRM, (void (*)(void *)) triggerCallback,
            /*userData*/getter);

    return 0; // 0 => success
}

int start(uint32_t numInPorts, uint32_t numOutPorts) {


    if(qsParameterGetterNumConnections(getter) == 0) return 0;

    // This block needs a start() function to setup and start the periodic
    // signal generator.
    errno = 0;
    struct itimerval it = {  { 1/*seconds*/, 0/*mictoseconds*/ }, { 0,0 }};
    if(setitimer(ITIMER_REAL, &it, 0)) {
        ERROR("setitimer(ITIMER_REAL,,) failed");
        return -1; // fail
    }

    return 0; // success
}

int stop(uint32_t numInPorts, uint32_t numOutPorts) {


    if(qsParameterGetterNumConnections(getter) == 0) return 0;

    // This block needs a stop() function to stop and remove the periodic
    // signal generator.
    if(setitimer(ITIMER_REAL, 0, 0)) {
        ERROR("setitimer(ITIMER_REAL,0,0) failed");
        return -1; // fail
    }

    return 0; // success
}
