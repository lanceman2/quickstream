// This was a generated file.
//
// This is the source to a quickstream Super Block module.

#include <string.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


struct QsBlockOptions options = { .type = QsBlockType_super };

// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)



int declare(void) {

    ///////////////////////////////////////////////////////////////
    //    Load child blocks
    ///////////////////////////////////////////////////////////////

    if(!qsGraph_createBlock(0, 0, "PipeOut", "PipeOut", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    if(qsBlock_makePortAlias(0, "PipeOut", "i", "0", "input"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    if(qsBlock_configVByName(0/*graph*/, "PipeOut",
             "Program", "QT_GUI_Sink",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "PipeOut",
             "AtStart", "true",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "PipeOut",
             "RelativePath", "quickstream/blocks/gnuradio-3.10.1",
             0/*null terminate*/))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
