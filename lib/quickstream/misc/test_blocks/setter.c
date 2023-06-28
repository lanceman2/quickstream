#include "../../../../include/quickstream.h"
#include "../../../debug.h"


int callback(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount, void *userData) {

    return 0;
}

int declare(void) {

    DSPEW();

    qsCreateSetter("mySetter", sizeof(double), QsValueType_double,
            0/*initValue*/, callback);

    return 0;
}
