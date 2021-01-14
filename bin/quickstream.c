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
            case -1:  // Bad opt
            case '*': // Bad opt
            case '?': // --help
            case 'h': // --help
            case 'V': //--version
                ProcessCommand(c, 0, argv[i], 0);
                break;

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
            case 'S': // --parameter-set BLOCK_NAME PARAMETER_NAME VALUE
                // We must have 3 additional args:
                if( (i+2) >= argc ||
                        (argv[i][0] == '-') ||
                        (argv[i+1][0] == '-')
                        ) {
                    fprintf(stderr, "--parameter-set without 3 args\n");
                    return 1;
                }
                if(!graph) {
                    // If there not graph than their are no blocks.
                    fprintf(stderr, "--parameter-set "
                            "with no blocks loaded");
                    return 1;
                }
                {
                    // Get the block
                    struct QsBlock *b = qsGraphGetBlockByName(graph,
                            argv[i]);
                    if(!b) return 1;
                    struct QsParameter *p = qsParameterGetPointer(b,
                            argv[i+1], false);

                    switch(qsParameterGetType(p)) {

                        // TODO: figure out how to do arrays.

                        case QsDouble:
                        {
                            char *endptr = 0;
                            double val = strtold(argv[i+2], &endptr);
                            if(argv[i+2] == endptr) {
                                fprintf(stderr, "--parameter-set %s "
                                        "%s %s\n"
                                        "  Failed to convert %s "
                                        "to double\n",
                                        argv[i], argv[i+1],
                                        argv[i+2], argv[i+2]);
                                return 1;
                            }
                            if(qsParameterSetValue(p, &val))
                                return 1; // error
                        }
                        break;
                        case QsString:
                        {
                            size_t len = strlen(argv[i+2]) + 1;
                            size_t psize = qsParameterGetSize(p);
                            char val[psize];
                            if(len > psize) {
                                strncpy(val, argv[i+2], psize);
                                val[psize - 1] = '\0';
                            } else
                                strcpy(val, argv[i+2]);
                            if(qsParameterSetValue(p, &val))
                                return 1; // error
                        }
                        break;
                        case QsSize:
                        {
                            char *endptr = 0;
                            size_t val = strtoul(argv[i+2], &endptr, 10);
                            if(argv[i+2] == endptr) {
                                fprintf(stderr, "--parameter-set %s "
                                        "%s %s\n"
                                        "  Failed to convert %s "
                                        "to unsigned long\n",
                                        argv[i], argv[i+1],
                                        argv[i+2], argv[i+2]);
                                return 1;
                            }
                            if(qsParameterSetValue(p, &val))
                                return 1; // error
                        }
                        break;
                        case QsNone:
                        case QsFloat:
                        case QsUint64:
                            ASSERT(0, "Write this code");
                        break;
                    }

                }
                // next
                arg = 0;
                i += 3; // 3 additional args parsed
                break;
            case 'c': // --connect block0 outPort block1 inPort
                // We must have 4 additional args:
                if( (i+3) >= argc ||
                        (argv[i][0] == '-') ||
                        (argv[i+1][0] == '-') ||
                        (argv[i+2][0] == '-') ||
                        (argv[i+3][0] == '-')
                        ) {
                    fprintf(stderr, "--connect "
                            "without 4 more args: "
                            "block0 outPort block1 inPort\n");
                    return 1;
                }
                if(!graph) {
                    // If there not graph than their are no blocks.
                    fprintf(stderr, "--connect "
                            "with no blocks loaded");
                    return 1;
                }
                char *end;
                uint32_t fromPortNum = strtoul(argv[i+1], &end, 10); 
                if(argv[i+1] == end) {
                    fprintf(stderr, "--connect bad outPort number: %s\n",
                            argv[i+1]);
                    ret = 1; // error
                    break;
                }
                uint32_t toPortNum = strtoul(argv[i+3], &end, 10); 
                if(argv[i+3] == end) {
                    fprintf(stderr, "--connect bad inPort number: %s\n",
                            argv[i+3]);
                    ret = 1; // error
                    break;
                }
                struct QsBlock *block0 =
                    qsGraphGetBlockByName(graph, argv[i]);
                if(!block0) {
                    fprintf(stderr, "--connect: block \"%s\" not found\n",
                            argv[i]);
                    ret = 1; // error
                    break;
                }
                struct QsBlock *block1 =
                    qsGraphGetBlockByName(graph, argv[i+2]);
                if(!block1) {
                    fprintf(stderr, "--connect: block \"%s\" not found\n",
                            argv[i+2]);
                    ret = 1;
                    break;
                }

                if(qsBlockConnect(block0, block1,
                            fromPortNum, toPortNum)) {
                    fprintf(stderr, "--connect: failed to connect\n");
                    ret = 1;
                    break;
                }

                // next
                arg = 0;
                i += 4; // 4 additional args parsed
                break;
            case 'C': // --connect-parameters
                // blockName0 parameterName0 blockName1 parameterName1
                // We must have 4 additional args:
                if( (i+3) >= argc ||
                        (argv[i][0] == '-') ||
                        (argv[i+1][0] == '-') ||
                        (argv[i+2][0] == '-') ||
                        (argv[i+3][0] == '-')
                        ) {
                    fprintf(stderr, "--connect-parameters "
                            "without 4 name args\n");
                    return 1;
                }
                if(!graph) {
                    // If there not graph than their are no blocks.
                    fprintf(stderr, "--connect-parameters "
                            "with no blocks loaded");
                    return 1;
                }
                // All these stupid block and parameter names must have a
                // match.
                //
                // The names must be of this form:
                //
                // example:
                //   --connect-parameters block0 par0 block1 par1
                {
                    struct QsBlock *b0 = qsGraphGetBlockByName(graph,
                            argv[i]);
                    // The from parameter must not be a setter.
                    struct QsParameter *p0 = b0?
                        qsParameterGetPointer(b0, argv[i+1],
                                false/*isSetter*/):0;

                    struct QsBlock *b1 = qsGraphGetBlockByName(graph,
                            argv[i+2]);
                    // If p0 is a constant parameter than p1 could be a
                    // setter or a constant.
                    struct QsParameter *p1 = b1?
                        qsParameterGetPointer(b1, argv[i+3],
                                /*isSetter*/true):0;
                    if(b1 && !p1 && qsParameterGetKind(p0) == QsConstant)
                        // Okay maybe it's a constant
                        p1 = qsParameterGetPointer(b1, argv[i+3],
                                /*isSetter*/false);

                    if(!b0 || !p0 || !b1 || !p1) {
                        fprintf(stderr, "--connect-parameters "
                                "%s %s %s %s FAILED: ",
                                argv[i], argv[i+1],
                                argv[i+2], argv[i+3]);
                        if(!b0)
                            fprintf(stderr, "Failed to find "
                                    "block named \"%s\" ",
                                    argv[i]);
                        else if(!p0)
                            fprintf(stderr, "Failed to find "
                                    "parameter named \"%s\" in block "
                                    "\"%s\" ",
                                    argv[i+1], argv[i]);
                        if(!b1)
                            fprintf(stderr, "Failed to find "
                                    "block named \"%s\" ",
                                    argv[i+2]);
                        else if(!p1)
                            fprintf(stderr, "Failed to find "
                                    "parameter named \"%s\" in block "
                                    "\"%s\" ",
                                    argv[i+3], argv[i+2]);
                        fprintf(stderr,"\n");
                        ret = 1; // fail
                        break;
                    }

                    //
                    // We can connect certain kinds of parameters from and
                    // to each other.  Here is the complete list of
                    // possible kinds of connections:
                    //
                    //   1. Getter    to  Setter
                    //   2. Constant  to  Setter
                    //   3. Constant  to  Constant
                    //
                    // There are only two kinds of groups of connections:
                    //
                    //   1.  1 Getter and 1 or more Setters
                    //   2.  1 or more Constants and 0 or more Setters
                    //
                    if(! (
                         (qsParameterGetKind(p0) == QsGetter &&
                            qsParameterGetKind(p1) == QsSetter) ||
                         (qsParameterGetKind(p0) == QsConstant &&
                            qsParameterGetKind(p1) == QsSetter) ||
                         (qsParameterGetKind(p0) == QsConstant &&
                            qsParameterGetKind(p1) == QsConstant)
                         )
                      ) {
                        fprintf(stderr,"--connect-parameters: Wrong mix "
                                "of parameter connections.\n");
                        ret = 1; // fail
                        break;
                    }
                    ret = qsParameterConnect(p0, p1);
                }
                // next
                arg = 0;
                i += 4; // 4 additional args parsed
                break;
            case 'P': // --parameters-print
                if(graph)
                    qsGraphParametersPrint(graph, stdout);
                break;
            case 'd': // --display Generate a graphviz dot graph and run the
                // imagemagick display program and continue running
                if(graph)
                    qsGraphPrintDotDisplay(graph, false/*waitForDisplay*/);
                break;
            case 'D': // --display-wait Generate a graphviz dot graph and run the
                // imagemagick display program and wait for the display
                // program to exit and then continue running
                if(graph)
                    qsGraphPrintDotDisplay(graph, true/*waitForDisplay*/);
                break;
            case 'o': // --dot  Print a graphviz dot file of the stream graph
                // to stdout
                if(graph)
                    qsGraphPrintDot(graph, stdout);
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
