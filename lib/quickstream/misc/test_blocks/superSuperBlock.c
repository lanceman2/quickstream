
#ifndef SPEW_LEVEL_DEBUG
#  define SPEW_LEVEL_DEBUG
#endif

#include "../../../../include/quickstream.h"
#include "../../../../include/quickstream_debug.h"


struct QsBlockOptions options = { .type = QsBlockType_super };


// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)


int declare(void) {


    if(!qsGraph_createBlock(0, 0, "superBlock2", "b0", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "superBlock2", "b1", 0))
        FAIL();

    return 0;
}
