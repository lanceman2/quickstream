#include <string.h>
#include <math.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


static struct QsParameter *getter;

// The gate is open (true) or not (false).
static bool gate = false;
static bool valueWaiting = false;
static bool value = false;


static
int SetGate(const struct QsParameter *p,
            const bool *newGate,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    if((gate = *newGate) && valueWaiting) {
        qsGetterPush(getter, &value);
        valueWaiting = false;
    }

    return 0;
}


static
int SetValue(const struct QsParameter *p,
            const bool *newValue,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    // Save the value.
    value = (*newValue)?true:false;

    if(gate)
        // The gate is open so we push a value even if the value did not
        // change from the last time.
        qsGetterPush(getter, &value);
    else
        valueWaiting = true;

    return 0;
}



int declare(void) {

    qsCreateSetter("gate",
        sizeof(gate), QsValueType_bool, &gate,
        (int (*)(const struct QsParameter *, const void *,
                uint32_t readCount, uint32_t queueCount,
                void *)) SetGate);

    qsCreateSetter("in",
        sizeof(value), QsValueType_bool, 0,
        (int (*)(const struct QsParameter *, const void *,
                uint32_t readCount, uint32_t queueCount,
                void *)) SetValue);

    getter = qsCreateGetter("out", sizeof(value),
            QsValueType_bool, 0);

    return 0;
}
