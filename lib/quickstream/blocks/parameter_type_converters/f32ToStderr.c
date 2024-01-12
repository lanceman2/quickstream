// TODO: Maybe make this a C++ template out of this so that there is
// one source file for both this and f64ToStderr.so; or use CPP (C
// preprocessor) code to macro-ize this (I'm not crazy about Large CPP
// macros).

#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../mprintf.h"


#define DEFAULT_FMT  "%g"
#define FMT_LEN      40


static char format[FMT_LEN];

static
int Value_setter(const struct QsParameter *p, float *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    fprintf(stderr, format, *value);
    fprintf(stderr, "\n");

    return 0;
}


// TODO: Format_config() is the same in f64ToStderr.c
static
char *Format_config(int argc, const char * const *argv,
            void *userData) {

    if(argc < 2)
        return QS_CONFIG_FAIL;

    strncpy(format, argv[1], FMT_LEN-1);
    format[FMT_LEN-1] = '\0';

    // mprintf() will malloc memory which libquickstream.so will own when
    // we return it to the function that called this callback.
    //
    // The string that we return will be in the command-line syntax for
    // the configure thingy with the current format value plugged into
    // it.
    return mprintf("format \"%s\"", format);
}



int declare(void) {

    qsCreateSetter("value",
        sizeof(float), QsValueType_float, 0/*0=no initial value*/,
        (int (*)(const struct QsParameter *, const void *,
            uint32_t readCount, uint32_t queueCount,
            void *)) Value_setter);

    // Initialize the format string.
    strncpy(format, DEFAULT_FMT, FMT_LEN);
    format[FMT_LEN - 1] = '\0';

    qsAddConfig((char *(*)(int argc, const char * const *argv,
            void *userData)) Format_config, "format",
            "set printf type format string"/*desc*/,
            "format FMT",
            "format " DEFAULT_FMT);

    DSPEW();
    return 0;
}
