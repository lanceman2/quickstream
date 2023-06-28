#include <signal.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"



int callback(void *userData) {

    DSPEW();

    return 0;
}



int declare(void) {

    DSPEW();

    qsSignalJobCreate(SIGRTMIN+4, callback, 0);
    qsSignalJobCreate(SIGRTMIN+5, callback, 0);

    return 0;
}
