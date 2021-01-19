// All this code does is parse arguments and call quickstream API
// functions, in a big switch.  Very straight forward.
//
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/debug.h"

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "quickstream.h"
#include "getOpt.h"
#include "../lib/quickstream/misc/qsOptions.h"



// Count the number of arguments up to one that starts with '-'
//
// i  is the current argv index.
//
// return the number of arguments after the first argument.
static inline
int GetNumArgs(int i, int argc, const char * const *argv) {

    int numArgs = 0;
    for(; i<argc && argv[i][0] != '-'; ++i)
        ++numArgs;
    return numArgs;
}



static void gdb_catcher(int signum) {
    // The action of ASSERT() depends on how this is compiled.
    ASSERT(0, "Caught signal %d\n", signum);
}


int main(int argc, const char * const *argv) {

    // Hang the program for debugging, if we segfault.
    signal(SIGSEGV, gdb_catcher);

    int i = 1;
    int ret = 0; // return code.
    const char *arg = 0;
    int numArgs; // dummy

    qsSetSpewLevel(DEFAULT_SPEW_LEVEL);

    while(i < argc) {

        int c = getOpt(argc, argv, &i, qsOptions, &arg);

        numArgs = GetNumArgs(i, argc, argv);

        // We used to have a long switch(c) here, but we moved
        // it into this function which we also use to run the --interpret
        // mode.
        ret = ProcessCommand(c, numArgs, argv[i], &argv[i]);
        i += numArgs;
        arg = 0;

        if(ret)
            // Something failed in a way that we can't continue to run.
            // We will skip parsing the rest of the arguments options.
            break;
    }


    if(graphs) {
        while(numGraphs)
            qsGraphDestroy(graphs[--numGraphs]);
        // Free the array of pointers.
        free(graphs);
    }

    // To free memory that was allocated by the spew and error CPP macro
    // functions: ASSERT(), DASSERT(), ERROR(), WARN(), NOTICE(), INFO(),
    // DSPEW() and SPEW() from debug.c and debug.h.
    //
    qsErrorFree();

    return ret;
}
