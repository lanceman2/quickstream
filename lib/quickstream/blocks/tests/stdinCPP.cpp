#include <stdio.h>
#include <string.h>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"


#define DEFAULT_OUT_MAX  ((size_t) -1)


class Stdin: public QsBlock {

    public:

    Stdin(void) {
        DSPEW();
    };


    int start(uint32_t numInputs, uint32_t numOutputs) {

        // This is a sink filter block
        ASSERT(numInputs == 0);
        ASSERT(numOutputs == 1);

        qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));

        outMax = DEFAULT_OUT_MAX;

        return 0;
    };


    int flow(void *buffers[], const size_t lens[],
            uint32_t numInputs, uint32_t numOutputs) {

        DASSERT(lens == 0);    // quickstream code error

        ASSERT(numInputs == 0);  // user error
        ASSERT(numOutputs); // user error

        size_t len = maxWrite;
        if(len > outMax)
            len = outMax;

        int done = 0;
        void *out0 = qsGetOutputBuffer(0);
        size_t ret = fread(out0, 1, maxWrite, stdin);
        qsOutput(0, ret);

        if(ret != maxWrite || feof(stdin)) {
            done = 1; // we are done
        }

        for(uint32_t i=1; i<numOutputs; ++i) {

            void *out = qsGetOutputBuffer(i);
            memcpy(out, out0, ret);
            qsOutput(i, ret);
        }

        outMax -= ret;
        if(outMax == 0) return 1; // done

        return done; // 0 for keep going.
    };

    ~Stdin(void) {
        DSPEW();
    };

    private:

    size_t maxWrite = 0; // write per flow() call

    size_t outMax = DEFAULT_OUT_MAX; // total

};


// QS_LOAD_BLOCK_MODULE will add the needed code to make this a loadable
// quickstream block module.
//
// We do not want a semicolon after this CPP macro
QS_LOAD_BLOCK_MODULE(Stdin)
