#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


bool isSuperBlock = true;

static int count = 0;


int declare(void) {

    DSPEW("count = %d", count++);


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
