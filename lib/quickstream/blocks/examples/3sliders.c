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
    "ZjY0U2V0dGVyc1RvR2V0dGVyAHNsaWRlcldpdGhGZWVkYmFja18zAHNsaWRlcldp"
    "dGhGZWVkYmFja18yAHNsaWRlcldpdGhGZWVkYmFjawAAAAAAAAAAOJpAAAAAAAC4"
    "nUCcAAAAAAAAAAAAAAAA9KJAAAAAAADInUDkAAAAAAAAAAAAAAAAOqFAAAAAAABI"
    "nUDkAAAAAAAAAAAAAAAA8J5AAAAAAADgnEDkAAAAAAAAAA==";

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

    if(!qsGraph_createBlock(0, 0, "examples/sliderWithFeedback.so", "sliderWithFeedback", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "examples/sliderWithFeedback.so", "sliderWithFeedback_2", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "examples/sliderWithFeedback.so", "sliderWithFeedback_3", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "parameter_type_converters/f64SettersToGetter.so", "f64SettersToGetter", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "f64SettersToGetter", "g", "out", "sliderWithFeedback", "s", "in"))
        FAIL();

    if(qsGraph_connectByStrings(0, "sliderWithFeedback", "s", "in", "sliderWithFeedback_2", "s", "in"))
        FAIL();

    if(qsGraph_connectByStrings(0, "sliderWithFeedback", "s", "in", "sliderWithFeedback_3", "s", "in"))
        FAIL();

    if(qsGraph_connectByStrings(0, "sliderWithFeedback", "g", "out", "f64SettersToGetter", "s", "0"))
        FAIL();

    if(qsGraph_connectByStrings(0, "sliderWithFeedback_2", "g", "out", "f64SettersToGetter", "s", "4"))
        FAIL();

    if(qsGraph_connectByStrings(0, "sliderWithFeedback_3", "g", "out", "f64SettersToGetter", "s", "1"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
