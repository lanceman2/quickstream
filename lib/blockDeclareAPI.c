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
    GET_BLOCK_IN_DECLARE(b);

    ASSERT(min <= max);

    b->minNumInputs = min;
    b->maxNumInputs = max;
    if(b->inputsEqualsOutputs) {
        b->minNumOutputs = min;
        b->maxNumOutputs = max;
    }
}


void qsBlockSetNumOutputs(uint32_t min, uint32_t max) {

    struct QsBlock *b = 0;
    GET_BLOCK_IN_DECLARE(b);

    ASSERT(min <= max);

    b->minNumOutputs = min;
    b->maxNumOutputs = max;
    if(b->inputsEqualsOutputs) {
        b->minNumInputs = min;
        b->maxNumInputs = max;
    }
}


void qsBlockSetNumInputsEqualsNumOutputs(bool inputsEqualsOutputs) {

    struct QsBlock *b = 0;
    GET_BLOCK_IN_DECLARE(b);

    // If the user set inputsEqualsOutputs to 2 and true is 1 then this
    // will make the value we save as true, which it 1.  So ya, bool could
    // screw you.
    //
    // I ask myself what is the value of (2 == true) in C?  That could be
    // somewhat ambiguous, and could be compiler dependent.  One can't
    // say, unless the C standard defines it.  I don't have a copy of all
    // compilers to test with.  So that is why I set
    // b->inputsEqualsOutputs to true or false and not just
    // inputsEqualsOutputs.
    b->inputsEqualsOutputs = inputsEqualsOutputs?true:false;


    // This is somewhat arbitrary:  We make the span of ports a minimum,
    // based on all the min and max values we have.
    //
    // TODO: This could burn the user, if they use a certain order of
    // calling qsBlockSetNumOutputs(), qsBlockSetNumInputs(), and
    // qsBlockSetNumInputsEqualsNumOutputs().
    //
    if(b->maxNumOutputs > b->maxNumInputs)
        b->maxNumOutputs = b->maxNumInputs;
    else if(b->maxNumInputs > b->maxNumOutputs)
        b->maxNumInputs = b->maxNumOutputs;

    if(b->minNumOutputs < b->minNumInputs)
        b->minNumOutputs = b->minNumInputs;
    else if(b->minNumInputs < b->minNumOutputs)
        b->minNumInputs = b->minNumOutputs;
}
