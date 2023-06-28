// Definitely just a test; so fucking wonky.

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


static struct QsParameter *getter = 0;

static uint64_t val = 0;

static uint64_t check = 100;


int Set(const struct QsParameter *p, uint64_t *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    ++(*value);
    ASSERT(val < *value);
    ASSERT(*value < val + 6);
    val = *value;

    if((*value) >= check) {
        printf("%" PRIu64 "\n", *value);
        check += 3000;
    }

    // We just need a value much larger than the control parameter queue
    // length, which is like 10 (see ../../parameter.c to check).
    if(*value >= 23179) {
        printf("\n");
        // qsStop() is an async function.
        qsStop();
        // This is it.  We are done running.
        // Well, at least after the async shit goes.
        WARN("Stopping stream");
        return 0;
    }

    qsGetterPush(getter, value);

    return 0;
}


int declare(void) {

    DSPEW();

    qsCreateSetter("add", sizeof(val), QsValueType_uint64,
            &val/*initValue*/,
            (int (*)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData)) Set);

    getter = qsCreateGetter("add", sizeof(val),
            QsValueType_uint64, 0);
    DASSERT(getter);


    return 0;
}
