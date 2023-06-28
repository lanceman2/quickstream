#include "../../../../include/quickstream.h"
#include "../../../debug.h"



int Set(const struct QsParameter *p, bool *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    if(*value) {
        INFO("Destroying graph");
        qsDestroy(0, 12/*status*/);
    }

    return 0;
}


int declare(void) {

    DSPEW();

    bool val = false;

    qsCreateSetter("destroy", sizeof(val), QsValueType_bool,
            &val/*initValue*/,
            (int (*)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData)) Set);

    return 0;
}
