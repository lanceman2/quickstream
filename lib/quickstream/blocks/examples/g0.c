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
    "RmlsZU91dABDaGVjazIAQ2hlY2sxAHNlcXVlbmNlR2VuAAAAAAAAAAAAADyiQAAA"
    "AAAAVqBA5AAAAAAAAAAAAAAAADyhQAAAAAAAfJtAbAAAAAAAAAAAAAAAADCeQAAA"
    "AAAAjJtA5AAAAAAAAAAAAAAAAHSaQAAAAAAABJtA5AAAAAAAAAA=";

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

    if(!qsGraph_createBlock(0, 0, "sequenceGen.so", "sequenceGen", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "sequenceCheck.so", "Check1", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "sequenceCheck.so", "Check2", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "FileOut", "FileOut", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    if(qsBlock_configVByName(0/*graph*/, "sequenceGen",
             "TotalOutputBytes", "0",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "sequenceGen",
             "Seeds", "2",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "Check1",
             "PassThrough", "t",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "Check1",
             "TotalOutputBytes", "0",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "Check1",
             "Seeds", "2",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "Check2",
             "TotalOutputBytes", "0",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "Check2",
             "Seeds", "2",
             0/*null terminate*/))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "sequenceGen", "o", "0", "Check1", "i", "0"))
        FAIL();

    if(qsGraph_connectByStrings(0, "Check1", "o", "0", "Check1", "i", "1"))
        FAIL();

    if(qsGraph_connectByStrings(0, "Check1", "o", "1", "Check1", "i", "2"))
        FAIL();

    if(qsGraph_connectByStrings(0, "Check2", "o", "0", "Check1", "i", "3"))
        FAIL();

    if(qsGraph_connectByStrings(0, "Check1", "o", "2", "Check2", "i", "0"))
        FAIL();

    if(qsGraph_connectByStrings(0, "Check1", "o", "3", "Check2", "i", "1"))
        FAIL();

    if(qsGraph_connectByStrings(0, "Check2", "o", "1", "FileOut", "i", "0"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
