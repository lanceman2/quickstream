/* If you are using this block you are likely doing something very silly.
 */

#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"

#define STR(s)   XSTR(s)
#define XSTR(s)  #s


#define DEFAULT_OUTPUTMAX   2048


static
char *InputMax_config(int argc, const char * const *argv,
        void *userData) {

    size_t outputMax = qsParseSizet(DEFAULT_OUTPUTMAX);
    if(outputMax == 0)
        outputMax = 1;

    DSPEW("Setting \"outputMax\" to %zu", outputMax);

    // These values set here get used at the next stream run.
    //
    qsSetInputMax(0/*port*/, 2*outputMax);
    qsSetOutputMax(0/*port*/, outputMax);

    return 0;
}



int declare(void) {

    // 1 input and 1 output.
    qsSetNumInputs(1/*min*/, 1/*max*/);
    qsSetNumOutputs(1/*min*/, 1/*max*/);


    qsSetInputMax(0/*port*/, 2*DEFAULT_OUTPUTMAX);
    qsSetOutputMax(0/*port*/, DEFAULT_OUTPUTMAX);

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) InputMax_config, "MaxWrite",
            "The maximum bytes requested for each stream output write",
            "MaxWrite NUM_BYTES",
            "MaxWrite " STR(DEFAULT_OUTPUTMAX));

    return 0;
}


int flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    DASSERT(numIn == 1);
    DASSERT(numOut == 1);

    if((*inLens) < 2 || !(*outLens))
        // We needed at least 2 byte in and 1 byte out.
        return 0;

    size_t len_out = *inLens/2;
    if(len_out > *outLens)
        len_out = *outLens;

    uint8_t *o = out[0];
    const char *end = in[0] + 2 * len_out;

    for(const char *i = in[0]; i < end; ++i, ++o) {

        // TODO: could be more efficient
        if('0' <= *i && *i <= '9')
            *o += *i - '0';
        else if('a' <= *i && *i <= 'f')
            *o += *i + 10 - 'a';
        else if('A' <= *i && *i <= 'F')
            *o += *i + 10 - 'A';
        else
            *o = 0;

        ++i;

        if('0' <= *i && *i <= '9')
            *o += 16 * (*i - '0');
        else if('a' <= *i && *i <= 'f')
            *o += 16 * (*i + 10 - 'a');
        else if('A' <= *i && *i <= 'F')
            *o += 16 * (*i + 10 - 'A');
    }

    qsAdvanceInput(0, 2 * len_out);
    qsAdvanceOutput(0, len_out);

    return 0;
}
