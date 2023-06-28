// This is kind-of dumb, using buffered IO to feed a buffered quickstream
// stream, but it is just a test so it's okay.  A better method to read
// stdin is to use unbuffered read.  Then again, if you pick the wrong
// buffer sizes this could end up faster than using unbuffered read.


#include "../../../debug.h"
#include "../../../../include/quickstream.h"


static
size_t maxWrite = 1045; 


int declare(void) {

    DSPEW();

    qsSetNumInputs(0, 0);
    qsSetNumOutputs(1, 1);

    return 0; // success
}

int start(uint32_t numIn, uint32_t numOut, void *userData) {

    DASSERT(numOut == 1);
    DASSERT(numIn == 0);

    qsSetOutputMax(0, maxWrite);

    return 0;
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {


    size_t len = outLens[0];

    if(!len) return 0;

    // This would be an API code error.
    // This should not happen given we have an output threshold.
    ASSERT(len, "Block \"%s\" had 0 bytes output available",
            __BASE_FILE__);


    size_t rd = fread(out[0], 1, len, stdin);

    if(rd)
        qsAdvanceOutput(0, rd);

    if(rd < len)
        return 1; // stdin is done

    return 0; // success
}

