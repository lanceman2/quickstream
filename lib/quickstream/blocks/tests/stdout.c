#include <stdio.h>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"


int declare(void) {

    return 0;
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    // This is a sink filter block
    ASSERT(numInputs);
    ASSERT(numOutputs == 0);

    return 0;
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(lens);    // quickstream code error
    DASSERT(lens[0]); // quickstream code error

    ASSERT(numInputs == 1);  // user error
    ASSERT(numOutputs == 0); // user error

    size_t len = lens[0];

    size_t ret = fwrite(buffers[0], 1, len, stdout);

    qsAdvanceInput(0, ret);

    //fflush(stdout);

    if(ret != len) {
        ERROR("fwrite(%p,1,%zu,stdout) failed",
            buffers[0], len);
        return -1; // fail
    }

    return 0; // success
}
