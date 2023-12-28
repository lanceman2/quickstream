// All this code does is parse arguments and call quickstream API
// functions in libquickstream.so, using a big switch statement.  Very
// straight forward, so we hope.
//
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/debug.h"

#include "../include/quickstream.h"

#include "./quickstream.h"

// defines qsOptions[] and DEFAULT_SPEW_LEVEL
#include "../lib/quickstream/misc/qsOptions.h"


int spewLevel = DEFAULT_SPEW_LEVEL;

bool exitOnError = false;


// Count the number of arguments up to one that starts with '-'
//
// i  is the current argv index.
//
// return the number of arguments after the i-th argument.
//
//
// Example:
//
//  $ quickstream -i --block stdin in --configure-mk MK foo bar -3 MK
//
//   i=1  returns 0
//   i=2  returns 2
//   i=5  returns 5
//
static inline
int GetNumArgs(int i, int argc, const char * const *argv,
        const char *command) {

    DASSERT(i < argc);
    DASSERT(argv[argc] == 0);

    int numArgs = 0;

    if(strcmp(command, "configure-mk") != 0 &&
            strcmp(command, "parameter-set-mk") != 0 &&
            strcmp(command, "add-metadata") != 0) {

        const char * const *arg = argv + i + 1;
        // argv is null terminated.
        for(; *arg && arg[0][0] != '-'; ++arg)
            ++numArgs;
        return numArgs;
    }

    // this should be: --configure-mk MK BLOCK_NAME ATTR_NAME [ARG0 ...] MK
    // Or:             --add-metadata MK KEY ARG0 [ARGS ...] MK
    // Or:             --parameter-set-mk MK BLOCK_NAME PNAME [ARGS ...] MK
    //
    // We need to have 5 args at least for these 3 commands.

    if(i + 5 > argc)
        // This will not be enough arguments.  We eat all the remaining
        // args and RunCommand() will handle the error.
        return argc - i;

    // go to MK, the marker if we can.  If the end marker is not found
    // this will eat all the rest of the arguments and RunCommand() will
    // deal with that error.

    numArgs = 1;
    const char *marker = *(argv + i + numArgs);
    ++numArgs;
    const char * const *arg = argv + i + numArgs;
    for(; *arg && strcmp(*arg, marker); ++arg)
        ++numArgs;

    if(!*arg) --numArgs;

    return numArgs;
}


static void gdb_catcher(int signum) {
    // The action of ASSERT() depends on how this is compiled.
    ASSERT(0, "Caught signal %d\n", signum);
}


int exitStatus = 0;


int main(int argc, const char * const *argv) {

    qsSetSpewLevel(spewLevel);

    // Hang the program for debugging, if we segfault.
    ASSERT(signal(SIGSEGV, gdb_catcher) != SIG_ERR);
    ASSERT(signal(SIGABRT, gdb_catcher) != SIG_ERR);


    int i = 1;
    int numArgs; // dummy
    const char *command = 0;

    while(i < argc) {

        // The returned command, c, is from the list in qsOptions from the
        // generated file ../lib/quickstream/misc/qsOptions.h
        // like: 
        //   struct opts qsOptions[] = {
        //       { "block", 'b' },
        //       { "block-help", 'B' },
        //       ...
        //       { 0, 0 }
        //   };
        //
        int c = getOpt(argc, argv, i, qsOptions, &command);

        // GetNumArgs() is more particular to this "quickstream program
        // than getOpt(), so that's why getOpt() does not set numArgs.
        //
        numArgs = GetNumArgs(i, argc, argv, command);


        // Process arguments:
        //
        switch(c) {

            case 0:
                DASSERT(0);
                break;

            case '*':
                if(spewLevel > 0)
                    fprintf(stderr, "Error: unknown option at: %s\n",
                            argv[i]);
                if(exitOnError) {
                    i = argc;
                    exitStatus = 1;
                    break;
                }
                ++i;
                break;

            default:

                // The question we are concerned with here is keep going
                // or not.  We let this other function do the harder
                // work in a much larger switch statement.
                if(RunCommand(c, numArgs+1, command, argv + i)) {
                    if(exitOnError) {
                        i = argc;
                        break;
                    }
                    else
                        exitStatus = 0;
                }
                ++i;
        }

        // Increment arguments:
        i += numArgs;
    }

    return exitStatus;
}
