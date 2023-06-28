#include "../../../../include/quickstream.h"
#include "../../../debug.h"


int declare(void) {

    DSPEW();

    return 0;
}

int undeclare(void *userData) {

    DSPEW();

    return -1; // less than 0 is a failure.
}
