#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../../include/quickstream_debug.h"



// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)


const struct QsBlockOptions options = { .type = QsBlockType_super };



int declare(void) {

    DSPEW();

    struct QsBlock *gen = qsGraph_createBlock(0, 0, "sequenceGen", "gen", 0);
    if(!gen)
        FAIL();

    struct QsBlock *check = qsGraph_createBlock(0, 0, "sequenceCheck", "check", 0);
    if(!check)
        FAIL();

    if(qsGraph_connectByBlock(0, gen, "o", "0", check, "i", "0"))
        FAIL();

    return 0;

}
