// This was a generated file that we edited.
//
// This is the source to a quickstream Super Block module.

#include <string.h>

#ifndef SPEW_LEVEL_DEBUG
#  define SPEW_LEVEL_DEBUG
#endif

#include "../../../../include/quickstream.h"
#include "../../../../include/quickstream_debug.h"


struct QsBlockOptions options = { .type = QsBlockType_super };


// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)


int declare(void) {

    ///////////////////////////////////////////////////////////////
    //    Load blocks
    ///////////////////////////////////////////////////////////////

    if(!qsGraph_createBlock(0, 0, "sequenceGen", "b0", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "passThroughCount", "b1", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "sequenceCheck", "b2", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "setterPrintUint64", "p", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases
    ///////////////////////////////////////////////////////////////

    if(qsBlock_makePortAlias(0, "b2", "i", "1", "in0"))
        FAIL();

    if(qsBlock_makePortAlias(0, "b2", "o", "0", "out"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Connect blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "b0", "o", "0", "b1", "i", "0"))
        FAIL();

    if(qsGraph_connectByStrings(0, "b1", "o", "0", "b2", "i", "0"))
        FAIL();

    if(qsGraph_connectByStrings(0, "b1", "g", "trigger", "p", "s", "value"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Configure blocks
    ///////////////////////////////////////////////////////////////

    if(qsBlock_configVByName(0/*graph*/, "b0",
             "TotalOutputBytes", "0",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "b1",
             "triggerCount", "100",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "b1",
             "quitCount", "8100",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "b2",
             "TotalOutputBytes", "0",
             0/*null terminate*/))
        FAIL();


    return 0;
}
