#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/block.h" // public interfaces

#include "debug.h"
#include "block.h"
#include "graph.h"
#include "GET_BLOCK.h"



int qsPassThroughBuffer(uint32_t inputPortNum, uint32_t outputPortNum) {

    struct QsBlock *b = 0;
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b; 

    // Make sure that we do not have conflict with a previous pass-through
    // list.
    for(uint32_t i = 0; i < smB->numPassThroughs; ++i) {
        if(smB->passThroughs[i].inputPortNum == inputPortNum) {
            ERROR("Block \"%s\" input port %" PRIu32
                    " is already a pass-through",
                    b->name, inputPortNum);
            return 1;
        }
        if(smB->passThroughs[i].outputPortNum == outputPortNum) {
            ERROR("Block \"%s\" output port %" PRIu32
                    " is already a pass-through",
                    b->name, outputPortNum);
            return 1;
        }
    }

    ++smB->numPassThroughs;
    smB->passThroughs = realloc(smB->passThroughs,
            smB->numPassThroughs*sizeof(*smB->passThroughs));
    ASSERT(smB->passThroughs, "realloc(,%zu) failed",
            smB->numPassThroughs*sizeof(*smB->passThroughs));
    smB->passThroughs[smB->numPassThroughs - 1].inputPortNum = inputPortNum;
    smB->passThroughs[smB->numPassThroughs - 1].outputPortNum = outputPortNum;

    return 0;
}


void qsAddRunFile(const char *filename, void *userData) {

    struct QsBlock *b = 0;
    GET_BLOCK_IN_DECLARE(b);
    ASSERT(b->runFilename == 0,
            "this can't be called twice in a block instance");

    b->runFilename = strdup(filename);
    ASSERT(b->runFilename, "strdup() failed");
    b->runFileUserData = userData;
}


// This function is only valid in a block callback function that is not
// declare()
//
void *qsRunFileData(void) {

    struct QsBlock *b = 0;
    GET_BLOCK_IN_NOT_DECLARE(b);

    return b->runFileUserData;
}


void qsBlockSetNumInputs(uint32_t min, uint32_t max) {

    struct QsBlock *b = 0;
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b; 

    ASSERT(min <= max);

    smB->minNumInputs = min;
    smB->maxNumInputs = max;
}


void qsBlockSetNumOutputs(uint32_t min, uint32_t max) {

    struct QsBlock *b = 0;
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b; 

    ASSERT(min <= max);

    smB->minNumOutputs = min;
    smB->maxNumOutputs = max;
}


void qsBlockSetNumInputsEqualsNumOutputs(bool inputsEqualsOutputs) {

    struct QsBlock *b = 0;
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b; 

    smB->inputsEqualsOutputs = inputsEqualsOutputs;
}
