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
    "ZW1wdHkAcnVuAGJ1dHRvbgBGaWxlT3V0AHNlcXVlbmNlR2VuAHNlcXVlbmNlQ2hl"
    "Y2tfNABzZXF1ZW5jZUNoZWNrXzMAc2VxdWVuY2VDaGVja18yAHNlcXVlbmNlQ2hl"
    "Y2sAAAAAAAAAAAAAALSZQAAAAAAABJhA5AAAAAAAAAAAAAAAACCkQAAAAAAAPJtA"
    "5AAAAAAAAAAAAAAAACSkQAAAAAAA1JhA5AAAAAAAAAAAAAAAAIKjQAAAAAAA5J1A"
    "5AAAAAAAAAAAAAAAAOCbQAAAAAAAOJlA5AAAAAAAAAAAAAAAAIqgQAAAAAAABJ5A"
    "yQAAAAAAAAAAAAAAAMydQAAAAAAA7J1A5AAAAAAAAAAAAAAAAGChQAAAAAAA1JhA"
    "bAAAAAAAAAAAAAAAAGifQAAAAAAAzJhA5AAAAAAAAAA=";

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

    FAIL_IF(!qsGraph_createBlock(0, 0, "sequenceCheck.so", "sequenceCheck", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "sequenceCheck.so", "sequenceCheck_2", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "sequenceCheck.so", "sequenceCheck_3", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "sequenceCheck.so", "sequenceCheck_4", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "sequenceGen.so", "sequenceGen", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "FileOut", "FileOut", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "gtk3/button.so", "button", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "runner/run.so", "run", 0));

    FAIL_IF(!qsGraph_createBlock(0, 0, "empty.so", "empty", 0));

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck",
             "PassThrough", "0", "1", "2", "3",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck",
             "TotalOutputBytes", "0",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck",
             "Seeds", "1",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck_2",
             "TotalOutputBytes", "0",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck_2",
             "Seeds", "1",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck_3",
             "TotalOutputBytes", "0",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck_3",
             "Seeds", "1",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck_4",
             "TotalOutputBytes", "0",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceCheck_4",
             "Seeds", "1",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceGen",
             "TotalOutputBytes", "0",
             0/*null terminate*/));

    FAIL_IF(qsBlock_configVByName(0/*graph*/, "sequenceGen",
             "Seeds", "1",
             0/*null terminate*/));

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceGen", "o", "0", "sequenceCheck", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceGen", "o", "1", "sequenceCheck", "i", "1"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck", "o", "0", "sequenceCheck", "i", "2"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck_4", "o", "0", "sequenceCheck", "i", "3"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck", "o", "1", "sequenceCheck_2", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck_3", "o", "0", "sequenceCheck_2", "i", "1"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck", "o", "2", "sequenceCheck_3", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck_2", "o", "0", "sequenceCheck_4", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck_2", "o", "1", "sequenceCheck_4", "i", "1"));

    FAIL_IF(qsGraph_connectByStrings(0, "sequenceCheck", "o", "3", "FileOut", "i", "0"));

    FAIL_IF(qsGraph_connectByStrings(0, "button", "g", "value", "run", "s", "run"));

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
