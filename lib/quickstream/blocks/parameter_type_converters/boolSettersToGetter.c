#include "../../../../include/quickstream.h"
#include "../../../debug.h"


static struct QsParameter *getter;


static
int Value_setter(const struct QsParameter *p, bool *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    // We just push the latest value.
    if(readCount == queueCount)
        qsGetterPush(getter, value);

    return 0;
}



int declare(void) {

    // Make 5 setters that are the similar.
    const char *names[] = {"4", "3", "2", "1", "0", 0 };
    for(const char * const *name = names; *name; ++name)
        qsCreateSetter(*name,
            sizeof(bool), QsValueType_bool, 0/*0=no initial value*/,
            (int (*)(const struct QsParameter *, const void *,
                uint32_t readCount, uint32_t queueCount,
                void *)) Value_setter);

    getter = qsCreateGetter("out",
            sizeof(bool), QsValueType_bool,
            0/*0=no initial value*/);

    DSPEW();
    return 0;
}
