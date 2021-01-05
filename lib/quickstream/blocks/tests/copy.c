#include <string.h>
#include <unistd.h>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#define DEFAULT_SEEDOFFSET ((size_t) 0)


static size_t maxWrite = 0;
static useconds_t usec = 0;


int declare(void) {

    double sec = 0;
     qsParameterConstantCreate(0, "sleep", QsDouble,
            sizeof(double),
            0, 0, &sec);



    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    ASSERT(numInputs);
    ASSERT(numInputs == numOutputs);

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));
    double sec = 0;
    qsParameterGetValueByName("sleep", &sec, sizeof(sec));
    usec = (sec * 1000000);

    return 0; // success
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    for(uint32_t i=0; i<numInputs; ++i) {

        size_t len = lens[i];
        if(len > maxWrite)
            len = maxWrite;
        else if(len == 0)
            continue;

        if(usec) usleep(usec);

        memcpy(qsGetOutputBuffer(i), buffers[i], len);
        qsAdvanceInput(i, len);
        qsOutput(i, len);
    }

    return 0;
}
