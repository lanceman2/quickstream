#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "quickstream.h"
#include "../lib/debug.h"

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "getOpt.h"
#include "../lib/quickstream/misc/qsOptions.h"


// The spew level.
// 0 == no spew, 1 == error, 2 == warn, 3 == notice, 4 == info, 5 == debug
//
// DEFAULT_SPEW_LEVEL is defined in
//   ../lib/quickstream/misc/quickstreamHelp.c.in
//   and goes to ../lib/quickstream/misc/qsOptions.h
//
int level = DEFAULT_SPEW_LEVEL;

// Current/last graph:
struct QsGraph *graph = 0;
//
uint32_t numGraphs = 0;
struct QsGraph **graphs = 0;



void CreateGraph(void) {

    graph = qsGraphCreate();
    ASSERT(graph);
    graphs = realloc(graphs, (++numGraphs)*sizeof(*graphs));
    ASSERT(graphs, "realloc(%p,%zu) failed", graphs, numGraphs*sizeof(*graphs));
    graphs[numGraphs-1] = graph;
}





// Process a single command.
//
// This does nothing much.  It just translates from a command string to
// a calls in the quickstream API, and maybe just a little bit more.
//
// If it's a command that terminates the program we exit here.
//
// ARGUMENTS:
//
// comm      is the command as a character.
//
// numArgs   is the number of arguments without the command.  Or length of
//           args array.   args is not necessarily null terminated.
//
// command   is the short or long name of the command.  If it's the long
//           name it's without the "--" in front.  If it's the short name
//           it's the same as comm but as a string like 'c' -> "c".
//
// args      arguments as strings from a command line option.
//
// pre       is "--" or "" depending on which parser is calling.
//
int ProcessCommand(int comm, int numArgs, const char *command,
        const char * const *argv) {

    switch(comm) {

        // We put the exiting cases before the non-exiting cases in this
        // switch.  Some errors cause an exit in what would be non-exiting
        // cases.

        /////////////////////////////////////////////////////////////////
        //               EXITING CASES                                 //
        /////////////////////////////////////////////////////////////////

        case -1:
        case '*':
            fprintf(stderr, "\nBad option: %s\n\n", command);
            // this will exit with error.
            exit(1);
        case '?':
        case 'h':
            // The --help option get stdout and all the error
            // cases get stderr.
            help(STDOUT_FILENO);
        case 'V': //--version
            printf("%s\n", QUICKSTREAM_VERSION);
            exit(0);

        /////////////////////////////////////////////////////////////////
        //           NON-EXITING CASES    (if no error)                //
        /////////////////////////////////////////////////////////////////

        case 'b': // --block FILENAME [BLOCK_NAME]
            break;

    }

    return 0;
}
