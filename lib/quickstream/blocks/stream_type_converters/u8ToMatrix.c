
#include <string.h>

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
    // make the read and write limits the same.
    //
    // These values set here get used at the next stream run.
    //
    qsSetInputMax(0/*port*/, inputMax);
    qsSetOutputMax(0/*port*/, inputMax);

    return 0;
}


// ASCII Output encoding:
const char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:;{}[]|\\!@#$%^&*()_+=/.,<>~abcdefghijklmnopqrstuvwxyz";
size_t CharsLen;

#define LINE_LENGTH  100

static
const size_t LineLength = LINE_LENGTH;

static size_t charCount = 0;

static
const uint8_t On = 0xfe, Off = 10;


static
bool showing[LINE_LENGTH];




int declare(void) {


    CharsLen = strlen(chars);
    memset(showing, 0, LineLength);

    // 1 input and 1 output.
    qsSetNumInputs(1/*min*/, 1/*max*/);
    qsSetNumOutputs(1/*min*/, 1/*max*/);


    qsSetInputMax(0/*port*/, DEFAULT_INPUTMAX);
    qsSetOutputMax(0/*port*/, DEFAULT_INPUTMAX);

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

    if(!*inLens || !(*outLens))
        // We needed at least 1 byte in and 1 byte out.
        return 0;

    size_t len = *outLens;
    if(len > (*inLens))
        len = (*inLens);

    char *o = out[0];
    const uint8_t *end = in[0] + len;


    for(const uint8_t *i = in[0]; i < end; ++i, ++o, ++charCount) {
        if(charCount >= LineLength) {
            *o = '\n';
            // reset the charCount
            charCount = -1;
            continue;
        }

        if(!showing[charCount] && *i >= On)
            showing[charCount] = true;
        else if(showing[charCount] && *i <= Off)
            showing[charCount] = false;

        if(showing[charCount])
            *o = chars[(*i)%CharsLen];
        else
            *o = ' ';
    }

    qsAdvanceInput(0, len);
    qsAdvanceOutput(0, len);

    return 0;
}
