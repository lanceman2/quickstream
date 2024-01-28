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
    "TnVsbFNpbmsARmlsZU91dAB1OFRvTWF0cml4AFJhbmRvbQAAAAAAAAAAAAAAvJlA"
    "AAAAAADQmkDkAAAAAAAAAAAAAAAAGqFAAAAAAABom0DkAAAAAAAAAAAAAAAAfJ5A"
    "AAAAAABsm0DkAAAAAAAAAAAAAAAApJtAAAAAAAA8m0DkAAAAAAAAAA==";

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

    FAIL_IF(!qsGraph_createBlock(0, 0, "FileIn", "Random", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "stream_type_converters/u8ToMatrix.so", "u8ToMatrix", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "FileOut", "FileOut", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "NullSink", "NullSink", 0));

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_makePortAlias(0, "NullSink", "i", "0", "input"));

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "Random",
             "Filename", "/dev/urandom",
             0/*null terminate*/));

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsGraph_connectByStrings(0, "Random", "o", "0", "u8ToMatrix", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "u8ToMatrix", "o", "0", "FileOut", "i", "0"));

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
