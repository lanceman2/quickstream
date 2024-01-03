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
    "TnVsbFNpbmsAcmFuZG9tIGJpdHMAAAAAAAAAAAAAAAAAIKFAAAAAAABQnEDkAAAA"
    "AAAAAAAAAAAALJ5AAAAAAAAUnEDkAAAAAAAAAA==";

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

    if(!qsGraph_createBlock(0, 0, "FileIn", "random bits", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "NullSink", "NullSink", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    if(qsBlock_configVByName(0/*graph*/, "random bits",
             "Filename", "/dev/urandom",
             0/*null terminate*/))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "random bits", "o", "0", "NullSink", "i", "0"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
