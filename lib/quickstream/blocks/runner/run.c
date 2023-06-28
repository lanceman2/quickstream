#include <signal.h>
#include <string.h>
#include <time.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"



static bool isRunning = 0;



int SetCallback(const struct QsParameter *p,
        const bool *value,
        uint32_t readCount, uint32_t queueCount,
        void *userData) {

    // Feedback for testing.
    DSPEW("Block \"%s\" Setter \"run\" got value[%" PRIu32 "]= %d",
            qsBlockGetName(), queueCount, *value);

    isRunning = (*value)?true:false;

    if(isRunning == qsIsRunning())
        // Something other then this changed the state,
        // or this is getting the same value again.
        return 0;

    if(isRunning)
        qsStart();
    else
        qsStop();

    return 0;
}


int declare(void) {

    qsCreateSetter("run",
        sizeof(isRunning), QsValueType_bool, 0/*initial value*/,
        (int (*)(const struct QsParameter *, const void *,
                uint32_t readCount, uint32_t queueCount,
                void *)) SetCallback);

    return 0;
}
