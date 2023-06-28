
#include "../../../../include/quickstream.h"
#include "../../../debug.h"

#define STR(s)   XSTR(s)
#define XSTR(s)  #s


#define DEFAULT_INPUTMAX   2048


static
char *InputMax_config(int argc, const char * const *argv,
        void *userData) {

    size_t inputMax = qsParseSizet(DEFAULT_INPUTMAX);
    if(inputMax == 0)
        inputMax = 1;

    DSPEW("Setting \"inputMax\" to %zu", inputMax);

    // Set the stream read and write limits for the next stream start.  We
    // make the read and write limits the same number of types.
    //
    // Input one uint8_t (unsigned byte) for each float (4 bytes) we
    // output.
    //
    // quickstream does not force this input to output ratio.  We are just
    // requesting that the stream ring buffers have these relative sizes
    // so that we might on average keep the input and output stream
    // buffers balanced and not a bottle neck on each other.  If we
    // connect to a slow writing block and the other end to a fast reading
    // block, we just get what we get.
    //
    // These values set here get used at the next stream run.
    //
    qsSetInputMax(0/*port*/, inputMax);
    qsSetOutputMax(0/*port*/, 4*inputMax);

    return 0;
}


int declare(void) {

    // Sanity checks.
    //
    // It may be better to use sizeof(float) in the code, but 4 is easier
    // to look at.
    DASSERT(4*sizeof(uint8_t) == sizeof(float));
    DASSERT(sizeof(uint8_t) == 1);

    // 1 input and 1 output.
    qsSetNumInputs(1/*min*/, 1/*max*/);
    qsSetNumOutputs(1/*min*/, 1/*max*/);


    qsSetInputMax(0/*port*/, DEFAULT_INPUTMAX);
    qsSetOutputMax(0/*port*/, 4*DEFAULT_INPUTMAX);

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) InputMax_config, "MaxRead",
            "The maximum bytes requested for each stream input read",
            "MaxRead NUM_BYTES",
            "MaxRead " STR(DEFAULT_INPUTMAX));

    return 0;
}


int flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    DASSERT(numIn == 1);
    DASSERT(numOut == 1);

    if(!*inLens || !(*outLens > 4))
        // We needed at least 1 byte in and 4 bytes out.
        return 0;

    size_t outLen = *outLens;
    if(outLen > (*inLens)*4)
        outLen = (*inLens)*4;
    size_t inLen = *inLens;
    if(inLen*4 > outLen)
        inLen = outLen/4;

    float *o = out[0];
    const uint8_t *end = in[0] + inLen;

    // uint8_t values vary from 0 to 255.
    //
    // Note: using int_8 will not give the same outputs as using uint8_t

    for(const uint8_t *i = in[0]; i < end; ++i, ++o)
        *o = (*i - 127.5)/127.5; // *o range is [-1, 1]

    // TODO: Q: Do we need to check for *o > 1.0 and *o < -1.0 due to
    // floating point round-off?

    qsAdvanceInput(0, inLen);
    qsAdvanceOutput(0, outLen);

    return 0;
}
