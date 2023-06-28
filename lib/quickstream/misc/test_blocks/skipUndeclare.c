#include "../../../../include/quickstream.h"
#include "../../../debug.h"


int declare(void) {

    DSPEW();

    return -1;
}

int undeclare(void *userData) {

    DSPEW();

    ASSERT(0, "This should not have been called");

    return 1;
}
