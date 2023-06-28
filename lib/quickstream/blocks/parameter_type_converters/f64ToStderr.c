#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../mprintf.h"



#define DEFAULT_FMT  "%lg"
#define FMT_LEN      54


static char format[FMT_LEN];

static
int Value_setter(const struct QsParameter *p, double *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    fprintf(stderr, format, *value);
    fprintf(stderr, "\n");

    return 0;
}


static
char *Format_config(int argc, const char * const *argv,
            void *userData) {

    if(argc < 2)
        return QS_CONFIG_FAIL;

    strncpy(format, argv[1], 31);
    format[31] = '\0';

    return mprintf("format \"%s\"", format);
}



int declare(void) {

    qsCreateSetter("value",
        sizeof(double), QsValueType_double, 0/*0=no initial value*/,
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
