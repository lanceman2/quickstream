// This is a test to make sure that if a block does not fore fill it's
// stream buffer write promise than it fails without overrunning the ring
// buffer or some shit like that.
//
// This test is kind-of stupid.  Then again one could argue that all
// coding logic tests are stupid.  But if this fails we got big problems.

#include <string.h>

#include "../../../../include/quickstream/block.h"
#include "../../../debug.h"

static size_t maxWrite = 0;

static
uint64_t totalOut = 8000, // default number of uint64_t per run.
        count, // total bytes out counter as we run.
        // When the total output reaches totalOut minus this amount we act
        // like an asshole and break our stream buffer output write
        // promise; like those fucking cock suckers that I currently work
        // with.
        failOffset = 101,
        // The ouput count we will try to try to break our write promise,
        // and try to make the stream fail.
        failCount;

static
bool failFlag;


// The value to write out for all output.  We don't care what it is; we
// just need a value.  0 is my hero.
static
uint64_t value = 0;


int declare(void) {

    qsParameterConstantCreate(0, "totalOut", QsUint64,
            sizeof(totalOut), 0/*setCallback*/, 0, &totalOut);


    return 0; // success
}


static
void AssertAction(FILE *stream, const char *file,
        int lineNum, const char *func) {

    if(strcmp(file, "blockFlowAPI.c") != 0 ||
        strcmp(func, "qsOutput") != 0) {
        ERROR("This test failed");
        exit(1);
    }
    // Note we are exiting without cleaning-up so this test will not
    // succeed if we run it with a valgrind memory leak check.
    ERROR("This test was successful.  We were supposed to ASSERT");
    exit(0);
}


int start(uint32_t numInputs, uint32_t numOutputs) {

    // This is a source filter block
    ASSERT(numInputs == 0);
    ASSERT(numOutputs);

    DSPEW("%" PRIu32 " outputs", numOutputs);

    // This better be true.
    ASSERT(failOffset <= totalOut);


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
    failFlag = false;

    failCount = totalOut - failOffset;

    return 0; // success
}


int flow(void *buffers[], const size_t lens[],
        uint32_t numInputs, uint32_t numOutputs) {

    DASSERT(numInputs == 0);
    DASSERT(numOutputs);

    if(failFlag) {
        // Unset the assert action callback that would have may this test
        // successful.  We fail instead.
        qsAssertAction = 0;
        // If we got to this point in the code we just found a very bad
        // bug in quickstream, which may cause the stream ring buffer to
        // be overrun.  Screw it all to hell.
        ASSERT(0, "THIS IS VERY BAD.  "
                "Stream ring buffer override may happen.");
    }

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

#if 1
    // This is to make it fail when this:
    if(count >= failCount) {
        // Now lets try to overrun the ring buffer by one item.
        len = maxWrite + sizeof(value);
        // Mark that we are trying to fail.
        failFlag = true;
        ERROR("Trying to fail: len=%zu >  maxWrite=%zu",
                len, maxWrite);
        // This sets the ASSERT() action callback to exit(0) for success,
        // assuming we fail due to qsOutput() in file blockFlowAPI.c.
        qsAssertAction = AssertAction;
    }
#endif

    DASSERT(len%sizeof(value) == 0);

    // Number of uint64_t ints.
    uint32_t numInts = len/sizeof(value);

    for(uint32_t i=0; i<numOutputs; ++i) {
        // TODO: use a vectorized version of this shit.  Like in the Volk
        // library, or a C version of a like thing.  For now fuck it.
        //
        uint64_t *out = qsGetOutputBuffer(i);
        for(size_t j=0; j<numInts; ++j, ++out)
            *out = value;

        // Advance the output buffer for port i by len bytes.
        qsOutput(i, len);
    }

    return ret;
}
