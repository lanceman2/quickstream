#include <signal.h>
#include <string.h>
#include <time.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


int SetCallback(const struct QsParameter *p,
        const bool *value,
        uint32_t readCount, uint32_t queueCount,
        void *userData) {

    // Feedback for testing.
    DSPEW("Block \"%s\" Setter \"destroy\" got value[%" PRIu32 "]= %d",
            qsBlockGetName(), queueCount, *value);

    if(*value)
        qsDestroy(0, 0);

    return 0;
}


int declare(void) {

    bool val = false;

    qsCreateSetter("destroy",
        sizeof(val), QsValueType_bool, &val,
        (int (*)(const struct QsParameter *, const void *,
                uint32_t readCount, uint32_t queueCount,
                void *)) SetCallback);

    return 0;
}
