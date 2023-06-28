
#include "../../debug.h"

#include "../../../include/quickstream.h"


// This defines built-in block "NullSink"

// We are in a C-like namespace of NullSink_
//
// symbols that are shared outside this file are prefixed with "NullSink_"
//
// make anything else that is file scope must be static.

#define MAX_INPUTS  5


int NullSink_declare(void) {

    // This block is a sink with at least one input stream
    qsSetNumInputs(1, MAX_INPUTS);

    return 0; // success
}


int NullSink_flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    DASSERT(numIn >= 1);
    DASSERT(numIn <= MAX_INPUTS);
    DASSERT(numOut == 0);

    for(uint32_t i = numIn - 1; i != -1; --i)
        if(inLens[i])
            // Advance all input that we got.
            qsAdvanceInput(0, inLens[i]);

    return 0; // success
}
