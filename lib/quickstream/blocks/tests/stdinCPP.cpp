#include <stdio.h>
#include <string.h>

#include "../../../../include/quickstream/block.h"
#include "../../../../include/quickstream/block.hpp"
#include "../../../../lib/debug.h"


#define DEFAULT_OUT_MAX  ((size_t) -1)


class Stdin: public QsBlock {

    public:

    Stdin(void) {
        DSPEW();
        qsBlockSetNumInputs(0,0);
        qsBlockSetNumOutputs(1,1);
    };


    int start(uint32_t numInputs, uint32_t numOutputs) {

        // This is a sink filter block
        DASSERT(numInputs == 0);
        DASSERT(numOutputs == 1);

        qsParameterGetValueByName("OutputMaxWrite", &maxWrite, sizeof(maxWrite));

        outMax = DEFAULT_OUT_MAX;

        return 0;
    };


    int flow(void *buffers[], const size_t lens[],
            uint32_t numInputs, uint32_t numOutputs) {

        DASSERT(lens == 0);    // quickstream code error

        DASSERT(numInputs == 0);  // user error
        DASSERT(numOutputs == 1); // user error

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
