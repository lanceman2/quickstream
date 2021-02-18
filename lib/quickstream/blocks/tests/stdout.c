#include <stdio.h>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"


int declare(void) {

    qsBlockSetNumInputs(1,10);
    qsBlockSetNumOutputs(0,0);
    return 0;
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    // This is a sink filter block
    DASSERT(numInputs);
    DASSERT(numOutputs == 0);

    return 0;
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(lens);    // quickstream code error
    DASSERT(lens[0]); // quickstream code error

    for(uint32_t i = 0; i < numInputs; ++i) {

        size_t len = lens[i];

        size_t ret = fwrite(buffers[i], 1, len, stdout);

        qsAdvanceInput(i, ret);

        if(ret != len) {
            ERROR("fwrite(%p,1,%zu,stdout) failed",
                buffers[i], len);
            return -1; // fail
        }
    }

    return 0; // success
}
