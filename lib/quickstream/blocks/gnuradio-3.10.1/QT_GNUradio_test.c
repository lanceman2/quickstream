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
    "dXJhbmRvbQB1OFRvRjMyAFFUX0dOVXJhZGlvX3NpbmsAAAAAAAAAAACMnEAAAAAA"
    "AOiZQGwAAAAAAAAAAAAAAAC8n0AAAAAAAJSaQOQAAAAAAAAAAAAAAABgoUAAAAAA"
    "AGiZQCcAAAAAAAAA";

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

    if(!qsGraph_createBlock(0, 0, "gnuradio-3.10.1/QT_GNUradio_sink.so", "QT_GNUradio_sink", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "stream_type_converters/u8ToF32.so", "u8ToF32", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "FileIn", "urandom", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    if(qsBlock_configVByName(0/*graph*/, "urandom",
             "Filename", "/dev/urandom",
             0/*null terminate*/))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "u8ToF32", "o", "0", "QT_GNUradio_sink", "i", "input"))
        FAIL();

    if(qsGraph_connectByStrings(0, "urandom", "o", "0", "u8ToF32", "i", "0"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
