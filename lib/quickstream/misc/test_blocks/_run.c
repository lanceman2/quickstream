// For test:  ../../../../tests/630_runFile

#include <string.h>
#include <unistd.h>

#define QS_USER_DATA_TYPE  int *

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


// For one with disableDSOLoadCopy not set test we load "B0" than "B1"
// than "B2"
//
// and 
//
// in another test with disableDSOLoadCopy set we load "b0" than "b1" than
// "b2"



int count = 0;


static void Check(int loadCount, int count) {

    const char *name = qsBlockGetName();
    ASSERT(name && name[0] && name[1] && !name[2]);

    ASSERT(name[0] == 'b' || name[0] == 'B');

    if(name[0] == 'B') {
        // Loaded with the copying of the DSOs
        // so we get different variables that have not
        // been 
        ASSERT(loadCount == 1);
        ASSERT(count == 0);
    } else {
        ASSERT(loadCount == 3,
                        "block \"%s\" has wrong loadCount=%d",
                        name, loadCount);
        // Loaded without the copying of the DSOs
        switch(name[1]) {
            case '0': // First time loaded in this graph 'b0'
                ASSERT(count == 0);
                break;
            case '1': // Second time loaded in this graph 'b1'
                ASSERT(count == 1);
                break;
            case '2': // Third time loaded in this graph 'b2'
                ASSERT(count == 2);
                break;
            default:
                ASSERT(0);
                break;
        }
    }
}


int construct( int *loadCount) {

    DSPEW("block \"%s\" count=%d", qsBlockGetName(), count);

    Check(*loadCount, count);

    ++count;

    ASSERT(count);

    return 0;
}


int start(uint32_t numInputs, uint32_t numOutputs, int *loadCount) {

    ASSERT(count);
    WARN();

    return 0; // success
}

int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        int *loadCount) {

    ASSERT(count);
    WARN();

    // We just needed this one flow() call per cycle.
    return 1;
}


int stop(uint32_t numInputs, uint32_t numOutputs, int *loadCount) {

    ASSERT(count);
    WARN();

    return 0;
}

int destroy(int *loadCount) {

    DSPEW("block \"%s\" count=%d", qsBlockGetName(), count);
    ASSERT(count);

    return 0;
}
