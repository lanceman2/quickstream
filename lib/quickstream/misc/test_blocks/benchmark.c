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
    "bnVsbFNvdXJjZQBudWxsU2luawBtdWx0QW5kQWRkX2Zsb2F0AAAAAAAAAAAAAAAA"
    "AIyYQAAAAAAAZJpA5AAAAAAAAAAAAAAAAAqgQAAAAAAAdJpA5AAAAAAAAAAAAAAA"
    "AMSbQAAAAAAAaJpA5AAAAAAAAAA=";

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

    FAIL_IF(!qsGraph_createBlock(0, 0, "multAndAdd_float.so", "multAndAdd_float", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "nullSink.so", "nullSink", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "nullSource.so", "nullSource", 0));

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "nullSource",
             "TotalOutputBytes", "10000000000",
             0/*null terminate*/));

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsGraph_connectByStrings(0, "nullSource", "o", "0", "multAndAdd_float", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "multAndAdd_float", "o", "0", "nullSink", "i", "0"));

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
