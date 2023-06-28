// This is not such a dumb idea, a sink block that writes to a standard
// buffered stream.  It could be efficient if the block feeding it is
// writing small chunks at a time.  In any case this is just a test.
//
#include "../../../debug.h"
#include "../../../../include/quickstream.h"


int declare(void) {

    qsSetNumInputs(1, 1);
    qsSetNumOutputs(0, 0);

    return 0; // success
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    if(!*inLens) return 0;

    ASSERT(*inLens, "inLens[0] == 0");

    size_t ret = fwrite(in[0], 1, *inLens, stdout);

    if(ret)
        // Eat this much input from port 0
        qsAdvanceInput(0, ret);

    if(ret != *inLens) {
        ERROR("fwrite()=%zu != %zu failed", ret, *inLens);
        return -1; // error
    }

    return 0; // success
}


int stop(uint32_t numIn, uint32_t numOut, void *userData) {

    fflush(stdout);

    return 0;
}

