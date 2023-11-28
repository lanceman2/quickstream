// This is a generated file.
//
// This is the source to a quickstream Super Block module.

#include <string.h>

#include <quickstream.h>
#include <quickstream_debug.h>


struct QsBlockOptions options = { .type = QsBlockType_super };

// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)


///////////////////////////////////////////////////////////////////
//  Block Meta Data
///////////////////////////////////////////////////////////////////
const char *QS_quickstreamGUI =
    "Z2F0ZQBub3QAc2xpZGVyAAAAAAAAAAAgnkAAAAAAALCcQLQAAAAAAAAAAAAAAAAI"
    "nEAAAAAAADSeQOQAAAAAAAAAAAAAAADQm0AAAAAAAAycQOQAAAAAAAAA";

// A null terminated array of all metaData symbol suffixes above:
const char *qsMetaDataKeys[] = {
    "quickstreamGUI",
    0 /*0 null terminator*/
};
///////////////////////////////////////////////////////////////////


int declare(void) {

    ///////////////////////////////////////////////////////////////
    //    Load child blocks
    ///////////////////////////////////////////////////////////////

    if(!qsGraph_createBlock(0, 0, "gtk3/slider.so", "slider", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "controlParameters/not.so", "not", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "controlParameters/gate.so", "gate", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    if(qsBlock_makePortAlias(0, "gate", "s", "in", "in"))
        FAIL();

    if(qsBlock_makePortAlias(0, "slider", "s", "show", "show"))
        FAIL();

    if(qsBlock_makePortAlias(0, "slider", "g", "value", "out"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "gate", "g", "out", "slider", "s", "value"))
        FAIL();

    if(qsGraph_connectByStrings(0, "slider", "g", "active", "not", "s", "in"))
        FAIL();

    if(qsGraph_connectByStrings(0, "not", "g", "out", "gate", "s", "gate"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
