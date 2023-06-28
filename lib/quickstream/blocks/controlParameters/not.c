#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


static struct QsParameter *getter;


static
int SetCallback(const struct QsParameter *p,
            const bool *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    const bool valueOut = !(*value);

    qsGetterPush(getter, &valueOut);

    return 0;
}


int declare(void) {

    bool val = false;

    qsCreateSetter("in",
        sizeof(val), QsValueType_bool, &val,
        (int (*)(const struct QsParameter *, const void *,
                uint32_t readCount, uint32_t queueCount,
                void *)) SetCallback);

    val = !val;

    getter = qsCreateGetter("out", sizeof(val),
            QsValueType_bool, &val);

    return 0;
}
