// For test:  ../../../../tests/630_runFile

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"
#include "../../../../lib/parseBool.h"


struct QsBlockOptions options = {
    .disableDSOLoadCopy = true
};


int loadCount = 0;


// For this test we load "b0" than "b1" than "b2"


int declare(void) {

    qsSetNumOutputs(1, 1);

    const char *name = qsBlockGetName();
    ASSERT(name && name[0] && name[1] && !name[2]);


    switch(name[1]) {

        case '0': // First time loaded in this graph
            ASSERT(loadCount == 0);
            break;
        case '1': // Second time loaded in this graph
            ASSERT(loadCount == 1);
            break;
        case '2': // Third time loaded in this graph
            ASSERT(loadCount == 2);
            break;
        default:
            ASSERT(0);
            break;
    }

    // Changing the value in all instances of this block from this DSO.
    ++loadCount;

    qsSetUserData(&loadCount);

    qsAddRunFile("_run.so", false/*do not load a copy*/);

    return 0; // success
}
