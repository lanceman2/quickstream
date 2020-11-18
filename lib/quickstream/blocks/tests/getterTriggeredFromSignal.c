#include <signal.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static int count = 0;


static
void triggerCallback(struct QsParameter *p) {


}


int bootstrap(struct QsGraph *graph) {

    DSPEW("count = %d", count++);

    
    struct QsParameter *getter = qsParameterGetterCreate(0, "getter",
            QS_DOUBLE, sizeof(double));

    qsTriggerSignalCreate(SIGALRM,
            (void (*)(void *userData)) triggerCallback, getter);

    return 0; // 0 => success
}

int start(uint32_t numInPorts, uint32_t numOutPorts) {

    // This block needs a start() function to setup and start the periodic
    // signal generator.

    return 0; // success
}

int stop(uint32_t numInPorts, uint32_t numOutPorts) {

    // This block needs a stop() function to stop and remove the periodic
    // signal generator.

    return 0; // success
}
