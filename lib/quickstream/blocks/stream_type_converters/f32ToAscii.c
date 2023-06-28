#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>


#include "../../../debug.h"
#include "../../../mprintf.h"
#include "../../../../include/quickstream.h"


#define STR(s)   XSTR(s)
#define XSTR(s)  #s


#define DEFAULT_INPUTMAX   2048

#define ASCII_FLOAT_LEN    20

#define MIN_INPUTMAX       (4 * ASCII_FLOAT_LEN)


//#define DEFAULT_EFMT       "%g\\n" // escaped format for "%g\n"
#define DEFAULT_EFMT       "%g\\n" // escaped format


#define FMT_LEN           10


// A printf() format for the float like "%g\n", "%5.5f ", or like.
static char format[FMT_LEN];


// Returns true on failure.
//
// Sets format[] on success.
//
// Fixes things like "\n" so that the 2 chars are turned into the 1 char
// ASCII character 10 ('\n').
//
// See 'man ascii'.
//
static inline bool DecodeFormat(const char *efmt) {

    DASSERT(efmt);
    DASSERT(efmt[0]);

    size_t len = 0;
    char nformat[FMT_LEN] = { 0 };
    char *nfmt = nformat;
    const char *c = efmt;
    for(; *c && len < FMT_LEN - 1; ++c, ++nfmt) {
        if(*c == '\\' && *c++) {
            // decode 1 char for 2 in if it begins with a '\\'
            switch(*c) {
                case 'n':
                    *nfmt = '\n'; // newline
                    break;
                case 't':
                    *nfmt = '\t'; // tab
                    break;
                default:
                    ERROR("Format \"%s\" was not decoded", efmt);
                    return true; // fail
            }
            continue;
        }
        // Take this char, *c, as it is.
        *nfmt = *c;
    }

    if(*c) return true; // It's too long.

    // Terminate it.
    *nfmt = '\0';

    DASSERT(strlen(nformat) > 1);

    // We could not write directly to format[] in case we failed, but
    // now we can.
    strcpy(format, nformat);

    DSPEW("format=\"%s\"", format);

    return false; // success
}



static
char *InputMax_config(int argc, const char * const *argv,
        void *userData) {

    size_t inputMax = qsParseSizet(DEFAULT_INPUTMAX);
    if(inputMax < MIN_INPUTMAX)
        inputMax = MIN_INPUTMAX;

    DSPEW("Setting \"inputMax\" to %zu", inputMax);

    //
    // This values set here get used at the next stream run.
    //
    qsSetInputMax(0/*port*/, inputMax);
    qsSetOutputMax(0/*port*/, 4*inputMax);

    return mprintf("InputMax %zu", inputMax);
}


static
char *Format_config(int argc, const char * const *argv,
        void *userData) {

    if(argc < 2 || !argv[1][0] || !argv[1][1])
        return QS_CONFIG_FAIL;

    if(DecodeFormat(argv[1]))
        return QS_CONFIG_FAIL;


    DSPEW("Setting \"Format\" to \"%s\" ==> \"%s\"", argv[1], format);

    return mprintf("Format %s", argv[1]);
}


int declare(void) {

    // Very stupid sanity check.  Imagine this not being true.  All C code
    // ever written would totally break.  But hay, this is an assumption
    // that is explicitly used in this code.  You see that if you ever do
    // larger character encodings than ASCII.  Types in C are kind-of
    // fucked up that way.
    //
    DASSERT(sizeof(char) == 1);

    // We get a run-time (ASSERT()) error if the default format[] is not a
    // good.  Then the first time this is run after DEFAULT_EFMT changed
    // DEFAULT_EFMT is checked.
    ASSERT(!DecodeFormat(DEFAULT_EFMT));


    // At least 1 input and 1 output, and up to:
    qsSetNumInputs(1/*min*/, 6/*max*/);
    qsSetNumOutputs(1/*min*/, 6/*max*/);


    qsSetInputMax(0/*port*/, DEFAULT_INPUTMAX);
    qsSetOutputMax(0/*port*/, 4*DEFAULT_INPUTMAX);

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) InputMax_config, "MaxRead",
            "The maximum bytes requested for each stream input read",
            "MaxRead NUM_BYTES",
            "MaxRead " STR(DEFAULT_INPUTMAX));

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) Format_config, "Format",
            "The printf print format to use for each "
            "float value output",
            "Format FLOAT_FORMAT",
            "Format " DEFAULT_EFMT);

    return 0;
}


static uint32_t num; // number of channels in and out.


int start(uint32_t numIn, uint32_t numOut, void *userData) {

    // libquickstream.so should not let these fail.
    DASSERT(numIn >= 1);
    DASSERT(numOut >= 1);

    // num is the lesser of numIn and numOut.
    //
    // An imbalance in in to out connections will lead to a stalled stream
    // port.
    //
    num = numIn;
    if(num > numOut) {
        num = numOut;
        ERROR("The last %" PRIu32 " stream input(s) will be stalled",
            numIn - numOut);
    } else if(numOut > num)
        ERROR("The last %" PRIu32 " stream output(s) will be stalled",
            numOut - numIn);

    DSPEW("%" PRIu32 " input(s) and output(s) will work format=\"%s\"",
            num, format);

    return 0;
}


int flow(const void * const in[], const size_t inLens[],
        uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    // libquickstream.so should not let these fail.
    //
    // TODO: Note that flow() is in a tight loop, so this check is not a
    // great thing here.
    DASSERT(numIn >= num);
    DASSERT(numOut >= num);

    // We don't know ahead of time that length of an output value string,
    // but if we do not have at least this much room, we will not even
    // try to write a value string to the output stream.
    const size_t valLen = ASCII_FLOAT_LEN;

    for(uint32_t i = 0; i < num; ++i) {
        // i is the port number for both input and output.

        if(inLens[i] < valLen || outLens[i] < sizeof(float))
            // Skip to the next input and output port.
            continue;

        // in bytes:
        size_t inLen = 0;
        size_t outLen = 0;

        const float *end = ((const float *)in[i]) + inLens[i]/sizeof(float);
        char *o = out[i];
        char *oEnd = o + outLens[i];

        for(const float *input = in[i]; input < end; ++input) {

            // TODO: snprintf() may not the fastest method to convert from
            // float to ASCII, but it's dam convenient.  It lets us vary
            // the format in a very standardized way.
            //
            size_t len = oEnd - o;
            if(len < valLen)
                // We are out of output room.
                break;
            int l = snprintf(o, len, format, *input);
            if(l >= len)
                // The output string buffer was not long enough.
                break;
            // Add another.
            inLen += sizeof(float);
            outLen += l;
            o += l;
        }
   
        if(inLen) {
            DASSERT(outLen);

            qsAdvanceInput(i, inLen);
            // Note: the string never has a '\0' null terminator in it in
            // this stream thingy.  It seems arbitrary as to where you'd
            // put null terminators in a stream.  So weird, C coding has
            // poisoned my brain into thinking that strings don't exist
            // without null terminators.  An ASCII file has no zeros in
            // it.  I just wrote a test program
            // ../../../../tests/_zerosInFile that shows this to be true.
            // See ../../../../tests/_zerosInFile.c
            //
            qsAdvanceOutput(i, outLen);
        }
    }

    return 0;
}


// TODO: We could use a flush() function for this block.

