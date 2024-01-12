#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../mprintf.h"


static struct QsParameter *getter;

static
int Value_setter(const struct QsParameter *p, double *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    float out = (* ((double *) value));

    qsGetterPush(getter, &out);

    return 0;
}


int declare(void) {

    // control parameter input is a double
    qsCreateSetter("value",
        sizeof(double), QsValueType_double, 0/*0=no initial value*/,
        (int (*)(const struct QsParameter *, const void *,
            uint32_t readCount, uint32_t queueCount,
            void *)) Value_setter);

    // We will output a control parameter that is a float.
    getter = qsCreateGetter("value",
            sizeof(float), QsValueType_float,
            0/*0=no initial value*/);

    DSPEW();
    return 0;
}
