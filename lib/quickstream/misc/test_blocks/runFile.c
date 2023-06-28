// For test:  ../../../../tests/630_runFile

#include "../../../../include/quickstream.h"
#include "../../../../lib/debug.h"



int loadCount = 0;


// For this test we load "B0" than "B1" than "B2"
//
// This DSO is loaded with a copy of the DSO.


int declare(void) {

    qsSetNumOutputs(1, 1);


    // We get our own copy of this loadCount variable.
    ASSERT(loadCount == 0);

    // Changing just in this instance of this block.
    ++loadCount;

    qsSetUserData(&loadCount);

    qsAddRunFile("_run.so", true);

    return 0; // success
}
