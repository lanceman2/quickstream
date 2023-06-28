#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"



static struct QsParameter *getter = 0;


static uint64_t num = 10000;
static uint64_t count;
static uint64_t total;
static uint64_t quitCount = 0;


static const size_t maxWrite = 423;

static
char *SetTriggerCount(int argc, const char * const *argv, void *userData) {

    qsParseUint64tArray(num, &num, 1);

    DSPEW("Triggering every %" PRIu64, num);

    return 0;
}


static
char *SetQuitCount(int argc, const char * const *argv, void *userData) {

    qsParseUint64tArray(quitCount, &quitCount, 1);

    DSPEW("Quit at count %" PRIu64, quitCount);

    return 0;
}



int declare(void) {

    qsSetNumInputs(1, 1);
    qsSetNumOutputs(0, 1);

    qsMakePassThroughBuffer(0, 0);

    getter = qsCreateGetter("trigger", sizeof(count),
            QsValueType_uint64, 0);
    DASSERT(getter);

    const size_t Len = 70;
    char text[Len];
    snprintf(text, Len, "triggerCount %" PRIu64, num);
    qsAddConfig(SetTriggerCount, "triggerCount",
            "Sends getter trigger every NUM",
            "triggerCount NUM",
            text);

    snprintf(text, Len, "quitCount %" PRIu64, num);
    qsAddConfig(SetQuitCount, "quitCount",
            "Calls qsStop() at count",
            "quitCount NUM",
            text);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs, void *userData) {

    ASSERT(numInputs);
    ASSERT(numOutputs <= 1);

    DSPEW("%" PRIu32 " inputs  %" PRIu32 " outputs",
            numInputs, numOutputs);

    qsSetOutputMax(0, maxWrite);
    if(numOutputs)
        qsSetInputMax(0, maxWrite);
    count = num;
    total = 0;

    return 0; // success
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void * const userData) {

    size_t len = inLens[0];
    if(numOut && len > outLens[0])
        len = outLens[0];
    if(len == 0)
        return 0;

    total += len;

    if(num && total >= count) {
        qsGetterPush(getter, &count);
        count += num;
    }

    if(quitCount && total >= quitCount) {
        DSPEW("Stopping stream");
        qsStop();
    }

    // Advance both the reader and the writer.
    //
    qsAdvanceInput(0, len);
    if(numOut)
        qsAdvanceOutput(0, len);

    return 0;
}
