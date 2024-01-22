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
    "ZjY0U2V0dGVyc1RvR2V0dGVyAHNsaWRlcl8zAHNsaWRlcl8yAHNsaWRlcgAAAAAA"
    "AAAAAAAAAHCfQAAAAAAASJpA5AAAAAAAAAAAAAAAAJqhQAAAAAAAUJpAtAAAAAAA"
    "AAAAAAAAADyZQAAAAAAAJJpAtAAAAAAAAAAAAAAAAFicQAAAAAAANJpAtAAAAAAA"
    "AAA=";

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

    FAIL_IF(!qsGraph_createBlock(0, 0, "gtk3/slider.so", "slider", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "gtk3/slider.so", "slider_2", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "gtk3/slider.so", "slider_3", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "parameter_type_converters/f64SettersToGetter.so", "f64SettersToGetter", 0));

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_makePortAlias(0, "slider", "s", "show", "show"));

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsGraph_connectByStrings(0, "f64SettersToGetter", "g", "out", "slider", "s", "display"));

    FAIL_IF(qsGraph_connectByStrings(0, "slider", "s", "display", "slider_2", "s", "display"));

    FAIL_IF(qsGraph_connectByStrings(0, "slider", "s", "display", "slider_3", "s", "display"));

    FAIL_IF(qsGraph_connectByStrings(0, "slider", "g", "value", "f64SettersToGetter", "s", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "slider_2", "g", "value", "f64SettersToGetter", "s", "4"));

    FAIL_IF(qsGraph_connectByStrings(0, "slider_3", "g", "value", "f64SettersToGetter", "s", "1"));

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
