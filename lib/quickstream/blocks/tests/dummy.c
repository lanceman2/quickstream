#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static int count = 0;


int declare(void) {

    count++;

    fprintf(stderr, "count = %d", count);

    qsBlockSetNumInputs(0,0);
    qsBlockSetNumOutputs(0,0);

    return 0; // 0 => success
}

int start(uint32_t numInPorts, uint32_t numOutPorts) {

    DSPEW();

    return 0; // success
}

int construct(void) {

    return 0; // success 
}


void destroy() {

    DSPEW();
}
