
#include "../../../include/quickstream/block.h"
#include "../../debug.h"

static size_t maxWrite = 0;

static
uint64_t totalOut = 8000, // default number of uint64_t per run.

        count; // total bytes out counter as we run.


// The value that we will write out repeatedly.
static
uint64_t value = 0;


static
int SetValueCB(struct QsParameter *p,
            uint64_t *value_in, void *userData) {

    value = *value_in;

    return 0;
}


int declare(void) {

    qsParameterSetterCreate(0/*this block*/, "value", 
            QsUint64, sizeof(value),
            (int (*)(struct QsParameter *p,
                const void *value, void *userData)) SetValueCB,
            0/*userData*/, &value);

    qsParameterConstantCreate(0, "totalOut", QsUint64,
            sizeof(totalOut),
            (int (*)(struct QsParameter *p,
                const void *value, void *userData)) SetValueCB,
            0, &totalOut);

    return 0; // success
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    // This is a source filter block
    ASSERT(numInputs == 0);
    ASSERT(numOutputs);

    DSPEW("%" PRIu32 " outputs", numOutputs);


    qsParameterGetValueByName("totalOut", &totalOut, sizeof(totalOut));

    if(totalOut == 0)
        // TODO: One could argue that this limit may be not good and make
        // very long running programs fail.  Kind of like this:
        // https://en.wikipedia.org/wiki/Year_2038_problem The data output
        // rate could be pretty large, so ya...
        // do the arithmetic.
        //
        totalOut = 0xFFFFFFFFFFFFFFFF/sizeof(value); // large amount
    //
    // totalOut is the maximum number of uint64_t integers written out,
    // but we change it to be in bytes here.

    totalOut = totalOut*sizeof(value); // now in bytes.

    // The constant parameter "OutputMaxWrite" is defined for all blocks.
    // Granted it may be a shit-fest if we have not all output ports not
    // evenly spewing.  Ya, we thought of that, so it can be more
    // complicated, but not in this source filter block.
    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));


    if(maxWrite%sizeof(value))
        // Make it the next highest multiple of sizeof(value).
        maxWrite -= maxWrite%sizeof(value) - sizeof(value);

    // We must be sure that all the write promises are the same size for
    // all output ports.  This may be not be true by default because
    // we may be changing it here.
    //
    for(uint32_t i=0; i<numOutputs; ++i)
        qsSetOutputMaxWrite(i, maxWrite);

    // Initialize the byte count.
    count = 0;

    return 0; // success
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(numInputs == 0);
    DASSERT(numOutputs);

    int ret = 0;

    size_t len = maxWrite;

    // All integer variables are in bytes now.

    // We watch for integer over wrap here, hence subtract and compare
    // not add and compare.
    if(len > totalOut - count) {
        ret = 1;
        len = totalOut - count;
        if(len == 0) return 1;
    }
    count += len;


    // Now switch to length in sizeof(value).  len is a multiple of
    // sizeof(value), assuming we did not fuck up.
    DASSERT(len%sizeof(value) == 0);

    len /= sizeof(value);

    for(uint32_t i=0; i<numOutputs; ++i) {
        // TODO: use a vectorized version of this shit.  Like in the Volk
        // library, or a C version of a like thing.  For now fuck it.
        //
        uint64_t *out = qsGetOutputBuffer(i);
        for(size_t j=0; j<len; ++j, ++out)
            *out = value;

        // Advance the output buffer for port i.
        qsOutput(i, len);
    }

    return ret;
}
