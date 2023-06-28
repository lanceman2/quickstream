#include "../../../../include/quickstream.h"
#include "../../../debug.h"


int callback(const struct QsParameter *p, const double *value,
            uint32_t readCount, uint32_t queueCount, void *userData) {

    printf("%lg\n", *value);

    return 0;
}

int declare(void) {

    DSPEW();

    qsCreateSetter("value", sizeof(double), QsValueType_double,
            0/*initValue*/,
            (int (*)(const struct QsParameter *p,
                     const void *value, uint32_t readCount, uint32_t queueCount,
                     void *userData)) callback);

    return 0;
}
