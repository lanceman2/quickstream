// Specifically for tests/505_startFail
//

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"


static
int startCount = 0;

static
int stopCount = 0;


int declare(void) {

    qsSetNumInputs(0,  0);
    qsSetNumOutputs(1, 2);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {


    DSPEW("Block \"%s\" %" PRIu32 " outputs",
            qsBlockGetName(), numOutputs);

    ++startCount;

    if(startCount <= 2)
        return 1; // non-zero => fail and do call stop()

    return -2; // non-zero => fail and do not call stop()
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    ASSERT(0, "We should never get here");

    return 0;
}


int stop(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    DSPEW();

    ASSERT(startCount <= 2, "startCount=%d", startCount);

    ++stopCount;
    
    return 0;
}


int destroy(void *userData) {

    DSPEW();
    //ASSERT(0, "This should not be called");

    return 0;
}


int undeclare(void *userData) {

    DSPEW("startCount=%d   stopCount=%d", startCount, stopCount);

    ASSERT(startCount == 3);
    ASSERT(stopCount == 2);


    return 0;
}
