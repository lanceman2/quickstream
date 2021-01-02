#include <stdio.h>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"


static size_t maxWrite = 0;


int declare(void) {

    return 0;
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    // This is a sink filter block
    ASSERT(numInputs == 0);
    ASSERT(numOutputs == 1);

    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));

    return 0;
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(lens == 0);    // quickstream code error

    ASSERT(numInputs == 0);  // user error
    ASSERT(numOutputs == 1); // user error

    void *out = qsGetOutputBuffer(0);

    size_t ret = fread(out, 1, maxWrite, stdin);

    qsOutput(0, ret);

    if(ret != maxWrite || feof(stdin)) {
        return 1; // we are done
    }

    return 0; // success
}
