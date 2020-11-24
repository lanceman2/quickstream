#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "../lib/debug.h"

#include "../include/quickstream/app.h"
#include "../include/quickstream/builder.h"

#include "getOpt.h"
#include "../lib/quickstream/misc/qsOptions.h"


// The spew level.
// 0 == no spew, 1 == error, 2 == warn, 3 == notice, 4 == info, 5 == debug
//
// DEFAULT_SPEW_LEVEL is defined in
//   ../lib/quickstream/misc/quickstreamHelp.c.in
static int level = DEFAULT_SPEW_LEVEL;

// Current graph:
static struct QsGraph *graph = 0;
// 
static uint32_t numGraphs = 0;
static struct QsGraph **graphs = 0;

static void CreateGraph(void) {

    graph = qsGraphCreate();
    ASSERT(graph);
    graphs = realloc(graphs, (++numGraphs)*sizeof(*graphs));
    ASSERT(graphs, "realloc(%p,%zu) failed", graphs, numGraphs*sizeof(*graphs));
    graphs[numGraphs-1] = graph;
}


// From quickstream_usage.c
// usage() does not return, it will exit.
extern void usage(int fd);


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


    qsSetSpewLevel(DEFAULT_SPEW_LEVEL);

    while(i < argc) {

        int c = getOpt(argc, argv, &i, qsOptions, &arg);

        switch(c) {

            // We put the exiting cases before the non-exiting cases in
            // this switch.  Some errors cause an exit in what would be
            // non-exiting cases.

            /////////////////////////////////////////////////////////////
            //               EXITING CASES                             //
            /////////////////////////////////////////////////////////////
            case -1:
            case '*':
                fprintf(stderr, "Bad option: %s\n\n", arg);
                // this will exit with error.
                return 1;
            case '?':
            case 'h':
                // The --help option get stdout and all the error
                // cases get stderr.
                usage(STDOUT_FILENO);
            case 'V': //--version
                printf("%s\n", QUICKSTREAM_VERSION);
                return 0;

            /////////////////////////////////////////////////////////////
            //           NON-EXITING CASES    (if no error)            //
            /////////////////////////////////////////////////////////////

            case 'b': // --block FILENAME [BLOCK_NAME]
                if(!arg) {
                    fprintf(stderr, "--block with no FILENAME\n");
                    return 1;
                }
                {
                    const char *name = 0;
                    if((i+1)<argc && argv[i+1][0] != '-') {
                        ++i;
                        name = argv[i];
                    }
                    if(!graph)
                        CreateGraph();
                    struct QsBlock *b = qsGraphBlockLoad(graph, arg, name);
                    if(!b) {
                        fprintf(stderr, "Loading block %s FAILED\n",
                                arg);
                        i = argc;
                        ret = 1; // error return code.
                        break; // fall out and cleanup.
                    }
                }
                // next
                arg = 0;
                ++i;
                break;
            case 'C': // --parameters-connect
                // We must have 4 additional args:
                if( (i+4) < argc ||
                        (argv[i+1][0] == '-') ||
                        (argv[i+2][0] == '-') ||
                        (argv[i+3][0] == '-') ||
                        (argv[i+4][0] == '-')
                        ) {
                    fprintf(stderr, "--parameters-connect "
                            "without 4 name args\n");
                    return 1;
                }
                if(!graph) {
                    // If there not graph than their are no blocks.
                    fprintf(stderr, "--parameters-connect "
                            "with no blocks loaded");
                    return 1;
                }
                // All these stupid block and parameter names must have a
                // match.
                //
                // The names must be of this form:
                //
                // example:
                //   --parameters-connect block0 par0 block1 par1
                {
                    struct QsBlock *b0 = qsGraphGetBlockByName(graph,
                            argv[i+1]);
                    // The from parameter must not be a setter.
                    struct QsParameter *p0 = b0?
                        qsParameterGetPointer(b0, argv[i+2],
                                false/*isSetter*/):0;

                    struct QsBlock *b1 = qsGraphGetBlockByName(graph,
                            argv[i+3]);
                    // If p0 is a constant parameter than p1 could be a
                    // setter or a constant.
                    struct QsParameter *p1 = b1?
                        qsParameterGetPointer(b1, argv[i+4],
                                /*isSetter*/true):0;
                    if(!p1 && qsParameterKind(p0) == QsConstant)
                        // Okay maybe it's a constant
                        p1 = qsParameterGetPointer(b1, argv[i+4],
                                /*isSetter*/false);


                    if(!b0 || !p0 || !b1 || !p1) {
                        fprintf(stderr, "--parameters-connect "
                                "%s %s %s %s FAILED: ",
                                argv[i+1], argv[i+2],
                                argv[i+3], argv[i+4]);
                        if(!b0)
                            fprintf(stderr, "Failed to find "
                                    "block named \"%s\" ",
                                    argv[i+1]);
                        else if(!p0)
                            fprintf(stderr, "Failed to find "
                                    "parameter named \"%s\" in block "
                                    "\"%s\" ",
                                    argv[i+2], argv[i+1]);
                        if(!b1)
                            fprintf(stderr, "Failed to find "
                                    "block named \"%s\" ",
                                    argv[i+3]);
                        else if(!p1)
                            fprintf(stderr, "Failed to find "
                                    "parameter named \"%s\" in block "
                                    "\"%s\" ",
                                    argv[i+4], argv[i+3]);
                        fprintf(stderr,"\n");
                        ret = 1; // fail
                        break;
                    }

                    // TODO: this is already checked in
                    // qsParameterConnect():
                    //
                    // We can connect certain kinds of parameters from and
                    // to each other.  Here is the complete list of
                    // possible kinds of connections:
                    //
                    //   1. Getter    to  Setter
                    //   2. Constant  to  Setter
                    //   3. Constant  to  Constant
                    //
                    if(!(qsParameterKind(p0) == QsGetter &&
                            qsParameterKind(p1) == QsSetter) ||
                        !(qsParameterKind(p0) == QsConstant &&
                            qsParameterKind(p1) == QsSetter) ||
                        !(qsParameterKind(p0) == QsConstant &&
                            qsParameterKind(p1) == QsConstant) 
                      ) {
                        fprintf(stderr,"--parameters-connect: Wrong mix "
                                "of parameter connections.\n");
                        ret = 1; // fail
                        break;
                    }

                    ret = qsParameterConnect(p0, p1);
                }
                // next
                arg = 0;
                i += 2; // 2 additional args for the 2 parameter names
                break;
            case 'P': // --parameters-print
                if(graph)
                    qsGraphParametersPrint(graph, stdout);
                break;
            case 'g': // --graph
                CreateGraph();
                break;
            case 's': // sleep SECONDS
                if(!arg) {
                    fprintf(stderr, "--sleep with no SECONDS\n");
                    return 1;
                }
                {
                    errno = 0;
                    char *endptr = 0;
                    double val = strtod(arg, &endptr);
                    if(endptr == arg || val <= 0.0) {
                        fprintf(stderr, "Bad --sleep option\n\n");
                        return 1;
                    }
                    if(level >= 4) // 4 = INFO
                        fprintf(stderr,"Sleeping %lg seconds\n", val);
                    usleep( (useconds_t) (val * 1000000.0));
                }
                // next
                arg = 0;
                ++i;
                break;
            case 'R': // --ready all the stream flow graphs
                for(uint32_t i = 0; i<numGraphs; ++i) {
                    ret = qsGraphReady(graphs[i]);
                    if(ret) break;
                }
                break;
            case 'r': // --run run all the stream flow graphs

                // TODO: We need to explore combinations of failure modes
                // for this flow/run sequence of calls.  Can we cleanup
                // while the stream is flowing?  What about when there is
                // more than one graph and only some graphs fail?

                for(uint32_t i = 0; i<numGraphs; ++i) {
                    ret = qsGraphRun(graphs[i]);
                    if(ret) break;
                }
                if(ret) break;
                //
                // Now wait for them to finish running:
                for(uint32_t i = 0; i<numGraphs; ++i) {
                    // For the case when there are no other threads than this
                    // main thread this will not block and do the right thing.
                    ret = qsGraphWait(graphs[i]);
                    if(ret < 0) break;
                }
                break;
            case 't': // --threads
                if(!arg) {
                    fprintf(stderr, "--threads with no MAX_THREADS\n");
                    return 1;
                }
                {
                    errno = 0;
                    char *endptr = 0;
                    uint32_t maxThreads = strtol(arg, &endptr, 10);
                    if(endptr == arg || maxThreads > 1000000) {
                        fprintf(stderr, "Bad --threads option\n\n");
                        return 1;
                    }
                    if(level >= 4) // 4 = INFO
                        fprintf(stderr,"Adding thread "
                                "pool with at most %" PRIu32
                                " threads\n", maxThreads);
                    if(!graph)
                        CreateGraph();
                    ASSERT(qsGraphThreadPoolCreate(graph, maxThreads));
                }
                // next
                arg = 0;
                ++i;
                break;
            case 'v': // --verbose
                if(!arg) {
                    fprintf(stderr, "--verbose with no level\n");
                    return 1;
                }
                {
                    // LEVEL maybe debug, info, notice, warn, error, and
                    // off which translates to: 5, 4, 3, 2, 1, and 0
                    // as this program (and not the API) define it.
                    if(*arg == 'd' || *arg == 'D' || *arg == '5')
                        // DEBUG 5
                        level = 5;
                    else if(*arg == 'i' || *arg == 'I' || *arg == '4')
                        // INFO 4
                        level = 4;
                    else if(*arg == 'n' || *arg == 'N' || *arg == '3')
                        // NOTICE 3
                        level = 3;
                    else if(*arg == 'w' || *arg == 'W' || *arg == '2')
                        // WARN 2
                        level = 2;
                    else if(*arg == 'e' || *arg == 'E' || *arg == '1')
                        // ERROR 1
                        level = 1;
                    else // none and anything else
                        // NONE 0 with a error string.
                        level = 0;

                    if(level >= 3/*notice*/)
                        fprintf(stderr, "quickstream spew level "
                                "set to %d\n"
                                "The highest libquickstream "
                                "spew level is %d\n",
                                level, qsGetLibSpewLevel());

                    qsSetSpewLevel(level);

                    // next
                    arg = 0;
                    ++i;
                    break;
                }

                default:
                // This should not happen, unless the options are not
                // coded correctly.  We are missing a case for this char.
                // This should be a case that is caught in case '*'.  That
                // means we have a short option char that is not in this
                // switch().
                ERROR("BAD CODE: Missing case for option character case: "
                        "%c opt=%s arg=%s\n", c, argv[i-1], arg);
                fprintf(stderr, "unknown option character: %c\n", c);
                // This will exit with error.  The program need more
                // coding.  We are totally screwed.
                return 1;
        }

        if(ret)
            // Something failed in a way that we can't continue to run.
            // We will skip parsing the rest of the arguments options.
            break;
    }

    if(graphs) {
        // a is a dummy QsGraph pointer iterator.
        struct QsGraph **a = graphs + (numGraphs - 1);
        while(a >= graphs) {
            qsGraphDestroy(*a);
            --a;
        }
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
