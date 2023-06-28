// This was a generated file, but we made some edits.
//
// This is the source to a quickstream Super Block module.

#include "../../../../include/quickstream.h"
#include "../../../../include/quickstream_debug.h"


struct QsBlockOptions options = { .type = QsBlockType_super };

// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)


///////////////////////////////////////////////////////////////////
//  Block Meta Data
///////////////////////////////////////////////////////////////////
const char *QS_key0 =
    "MTIzNAB0aGlzAGlzAG1ldGFkYXRhADEyMzQAAA==";
const char *QS_key1 =
    "NDMyMQB0aGlzAGlzAG1vcmUAbWV0YWRhdGEANDMyMQAA";

// A null terminated array of all metaData symbol suffixes above:
const char *qsMetaDataKeys[] = {
    "key0",
    "key1",
    0 /*0 null terminator*/
};
///////////////////////////////////////////////////////////////////


int declare(void) {

    ///////////////////////////////////////////////////////////////
    //    Load blocks
    ///////////////////////////////////////////////////////////////

    if(!qsGraph_createBlock(0, 0, "sequenceGen", "b0", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "sequenceCheck", "b1", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases
    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////
    //    Connect blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "b0", "o", "0", "b1", "i", "0"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Configure blocks
    ///////////////////////////////////////////////////////////////


    return 0;
}
