#include "../../../../include/quickstream.h"
#include "../../../debug.h"


int callback(const struct QsParameter *p, const uint64_t *value,
            uint32_t readCount, uint32_t queueCount, void *userData) {

    printf("%" PRIu64 "\n", *value);

    return 0;
}

int declare(void) {

    DSPEW();

    qsCreateSetter("value", sizeof(uint64_t), QsValueType_uint64,
            0/*initValue*/,
            (int (*)(const struct QsParameter *p,
                     const void *value, uint32_t readCount,
                     uint32_t queueCount,
                     void *userData)) callback);

    return 0;
}
