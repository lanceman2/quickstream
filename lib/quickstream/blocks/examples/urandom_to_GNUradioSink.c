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
    "VVJhbmRvbQB1OFRvRjMyAFFUX0dOVXJhZGlvX3NpbmsAAAAAAAAAAACYnkAAAAAA"
    "APiaQGwAAAAAAAAAAAAAAADkoEAAAAAAAKSbQOQAAAAAAAAAAAAAAAA2okAAAAAA"
    "AMiaQCcAAAAAAAAA";

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

    FAIL_IF(!qsGraph_createBlock(0, 0, "gnuradio-3.10.1/QT_GNUradio_sink.so", "QT_GNUradio_sink", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "stream_type_converters/u8ToF32.so", "u8ToF32", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "FileIn", "URandom", 0));

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "URandom",
             "Filename", "/dev/urandom",
             0/*null terminate*/));

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsGraph_connectByStrings(0, "u8ToF32", "o", "0", "QT_GNUradio_sink", "i", "input"));

    FAIL_IF(qsGraph_connectByStrings(0, "URandom", "o", "0", "u8ToF32", "i", "0"));

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
