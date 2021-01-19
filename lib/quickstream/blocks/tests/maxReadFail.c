// This is a test to make sure that if a block does not fore fill it's
// stream buffer read promise.
//
// This test is kind-of stupid.  Then again one could argue that all
// coding logic tests are stupid.  But if this fails we got big
// problems.

#include <string.h>

#include "../../../../include/quickstream/block.h"
#include "../../../debug.h"

static size_t maxWrite = 0;

static
uint64_t totalOut = 10000, // default number of uint64_t per run.
        count, // total bytes out counter for port 0 as we run.
        // When the total output reaches totalOut minus this amount we act
        // like an asshole and break our stream buffer output write
        // promise; like those fucking cock suckers that I currently work
        // with.
        failOffset = 107,
        // The ouput count we will try to try to break our write promise,
        // and try to make the stream fail.
        failCount;

static
bool failFlag;



int declare(void) {

    qsParameterConstantCreate(0, "totalOut", QsUint64,
            sizeof(totalOut), 0/*setCallback*/, 0, &totalOut);


    return 0; // success
}

#if 1
static
void AssertAction(FILE *stream, const char *file,
        int lineNum, const char *func) {

    if(strcmp(file, "stream.c") != 0) {
        // We could not check the function name because it's an inline
        // function and so that will change the function name depending on
        // the compiler and compiler options.  This test is better than no
        // test at all.
        ERROR("This test failed");
        exit(1); // != 0 => failure
    }
    // Note: we are exiting without cleaning-up so this test will not
    // succeed if we run it with a valgrind memory leak check.
    ERROR("This test was successful.  We were supposed to ASSERT");
    exit(0); // 0 = success.
}
#endif


int start(uint32_t numInputs, uint32_t numOutputs) {

    ASSERT(numInputs != 0);
    ASSERT(numOutputs == numInputs);

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
        totalOut = 0xFFFFFFFFFFFFFFFF; // large amount
    //
    // totalOut is the maximum number of uint64_t integers written out,
    // but we change it to be in bytes here.

    // The constant parameter "OutputMaxWrite" is defined for all blocks.
    // Granted it may be a shit-fest if we have not all output ports not
    // evenly spewing.  Ya, we thought of that, so it can be more
    // complicated, but not in this source filter block.
    qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));


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

    DASSERT(numInputs != 0);
    DASSERT(numOutputs == numInputs);

    if(failFlag) {
        // Unset the assert action callback that would have may this test
        // successful.  We fail instead.
        qsAssertAction = 0;
        // If we got to this point in the code we just found a very bad
        // bug in quickstream, which may cause the stream ring buffer to
        // be clogged.
        ASSERT(0, "THIS IS VERY BAD.  "
                "The stream may get clogged due to a reader"
                " filter block not reading as promised.");
    }

    for(uint32_t i=0; i<numInputs; ++i) {

        size_t len = lens[i];
        if(len > maxWrite)
            len = maxWrite;
        else if(len == 0)
            continue;

        if(i==0) {
            count += len;
            if(count >= failCount) {
                failFlag = true;
                qsAssertAction = AssertAction;
                return 0;
            }
            ERROR();
        }

        memcpy(qsGetOutputBuffer(i), buffers[i], len);
        qsAdvanceInput(i, len);
        qsOutput(i, len);
    }

    return 0;
}
