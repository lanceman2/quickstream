// This is a generated file.
//
// This is the source to a quickstream Super Block module.

#include <string.h>

#include <quickstream.h>
#include <quickstream_debug.h>


struct QsBlockOptions options = { .type = QsBlockType_super };

// ERROR() will print a line number which is a big help.
#define FAIL_IF(x)   do {if( (x) ) { ERROR(); return -1; } } while(0)


///////////////////////////////////////////////////////////////////
//  Block Meta Data
///////////////////////////////////////////////////////////////////
const char *QS_quickstreamGUI =
    "SGV4ZHVtcABVUmFuZG9tAAAAAAAAAAAcoEAAAAAAAIyaQOQAAAAAAAAAAAAAAAC0"
    "nEAAAAAAAGyaQOQAAAAAAAAA";

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

    FAIL_IF(!qsGraph_createBlock(0, 0, "FileIn", "URandom", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "PipeOut", "Hexdump", 0));

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "URandom",
             "Filename", "/dev/urandom",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "Hexdump",
             "Program", "hexdump", "-v",
             0/*null terminate*/));

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsGraph_connectByStrings(0, "URandom", "o", "0", "Hexdump", "i", "0"));

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
