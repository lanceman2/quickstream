#include "../../../../include/quickstream.h"
#include "../../../debug.h"

#define TEST_VALUE  (0xFE34589)


static struct UserData {

    int value;
}
userData = { .value = TEST_VALUE };


static
int SetBool(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    DSPEW("setting bool setter to %s", (*(bool*) value)?"true":"false");

    ASSERT(((struct UserData *) userData)->value == TEST_VALUE);

    return 0;
}


static
int SetUint64(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    DSPEW("setting uint64 setter to %" PRIu64, * (uint64_t *) value);

    ASSERT(((struct UserData *) userData)->value == TEST_VALUE);

    return 0;
}


static
int Set3Double(const struct QsParameter *p, const double *val,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    DSPEW("setting double setter to %lg, %lg, %lg", val[0], val[1], val[2]);

    ASSERT(((struct UserData *) userData)->value == TEST_VALUE);

    return 0;
}



static
char *config(int argc, const char * const *argv, void *userData) {

    fprintf(stderr, "%s:%s( %d/*argc*/, {",__BASE_FILE__, __func__, argc);
    for(const char * const *arg = argv; *arg; ++arg)
        fprintf(stderr," \"%s\",", *arg);
    fprintf(stderr, " 0}, %p/*userData*/)\n", userData);

    ASSERT(((struct UserData *) userData)->value == TEST_VALUE);

    double vals[5];

    qsParseDoubleArray(0.123, vals, 5);

    fprintf(stderr, "%s Parsed 5 double values: %lg, %lg, %lg, %lg, %lg\n",
            argv[0], vals[0], vals[1], vals[2], vals[3], vals[4]);


    return 0;
}


int declare(void) {

    DSPEW();
    uint64_t freq = 0;

    qsSetUserData(&userData);

    qsSetNumInputs(0, 4);
    qsSetNumOutputs(0, 4);

    qsMakePassThroughBuffer(0, 0);


    qsAddConfig(config, "config1"/*name*/, "desc1"/*desc*/,
            "argSyntax1"/*argSyntax*/, "defaultArgs1"/*defaultArgs*/);

    qsAddConfig(config, "config2"/*name*/, "desc2"/*desc*/,
            "argSyntax2"/*argSyntax*/, "defaultArgs2"/*defaultArgs*/);


    qsCreateSetter("freq1", sizeof(freq), QsValueType_uint64,
            &freq/*initValue*/, SetUint64);

    qsCreateGetter("freq1", sizeof(freq), QsValueType_uint64, 0);

    qsCreateSetter("freq2", sizeof(freq), QsValueType_uint64,
            &freq/*initValue*/, SetUint64);

    qsCreateGetter("freq2", sizeof(freq), QsValueType_uint64, 0);

    qsCreateSetter("freq3", sizeof(freq), QsValueType_uint64,
            &freq/*initValue*/, SetUint64);

    qsCreateGetter("freq3", sizeof(freq), QsValueType_uint64, 0);


    double gain[3] = { 0, 0, 0 };


    qsCreateSetter("gain", sizeof(gain), QsValueType_double,
            &gain/*initValue*/,
            (int (*) (const struct QsParameter *, const void *,
                      uint32_t, uint32_t, void *)) Set3Double);

    qsCreateGetter("gain", sizeof(gain), QsValueType_double, 0);


    bool on = false;

    qsCreateSetter("on", sizeof(on), QsValueType_bool,
            &on/*initValue*/, SetBool);

    qsCreateGetter("on", sizeof(on), QsValueType_bool, 0);



    return 0;
}
