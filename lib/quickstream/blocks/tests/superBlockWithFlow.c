#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


bool isSuperBlock = true;

static int count = 0;


int bootstrap(void) {

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

int flow(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs) {

    return 0;
}

void destroy() {

    DSPEW();
}
