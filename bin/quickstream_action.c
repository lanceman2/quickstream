#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "quickstream.h"
#include "../lib/debug.h"
#include "../lib/parameter.h"

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


static void PrintInvalidCommand(int numArgs, const char *command,
        const char * const *argv) {

    fprintf(stderr, "Invalid command: %s ", command);
    for(int i=0; i<numArgs; ++i)
        fprintf(stderr, "%s ", argv[i]);
    putc('\n', stderr);
}


int ParameterSet(const char *blockName, const char *parameterName,
        const char * const *values, int numValues) {

    // Get the block
    struct QsBlock *b = qsGraphGetBlockByName(graph, blockName);
    if(!b) return 1;
    struct QsParameter *p =
        qsParameterGetPointer(b, parameterName, false);
    if(!p) {
        ERROR("block \"%s\" parameter \"%s\" not found",
                blockName, parameterName);
        return 1; // fail
    }

    switch(qsParameterGetType(p)) {

        // TODO: figure out how to do arrays.

        case QsDouble:
        {
            char *endptr = 0;
            double val = strtold(*values, &endptr);
            if(*values== endptr) {
                fprintf(stderr, "--parameter-set %s %s %s\n"
                            "  Failed to convert %s "
                            "to double\n",
                            blockName, parameterName,
                            *values, *values);
                return 1;
            }
            if(qsParameterSetValue(p, &val))
                return 1; // error
            }
            break;

        case QsString:
        {
            size_t len = strlen(*values) + 1;
            size_t psize = qsParameterGetSize(p);
            char str[psize];
            if(len > psize) {
                strncpy(str, *values, psize);
                str[psize - 1] = '\0';
            } else
                strcpy(str, *values);
            if(qsParameterSetValue(p, str))
                return 1; // error
        }
            break;

        case QsSize:
        {
            char *endptr = 0;
            size_t val = strtoul(*values, &endptr, 10);
            if(*values == endptr) {
                fprintf(stderr, "--parameter-set %s %s %s\n"
                        "  Failed to convert %s "
                        "to unsigned long\n",
                        blockName, parameterName,
                            *values, *values);
                return 1;
            }
            if(qsParameterSetValue(p, &val))
                return 1; // error
        }
            break;

        case QsUint64:
        {
            char *endptr = 0;
            uint64_t val = strtoull(*values, &endptr, 10);
            DASSERT(p->size == sizeof(val));
            if(*values == endptr) {
                fprintf(stderr, "--parameter-set %s %s %s\n"
                        "  Failed to convert %s "
                        "to unsigned long long\n",
                        blockName, parameterName,
                            *values, *values);
                return 1;
            }
            if(qsParameterSetValue(p, &val))
                return 1; // error
        }
            break;

        case QsFloat:
        case QsNone:
            ASSERT(0, "Write this code");
            break;
    }
    return 0;
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
// argv      arguments as strings for the command.
//
//
// return 0 on success and non-zero on error
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

            // We do not want the name to start with '-'
            if(!numArgs || argv[0][0] == '-' || numArgs > 2) {
                PrintInvalidCommand(numArgs, command, argv);
                return 1;
            }
            {
                if(!graph)
                    CreateGraph();
                const char *name = 0;
                if(numArgs > 1)
                    name = argv[1];
                struct QsBlock *b = qsGraphBlockLoad(graph, argv[0], name);
                if(!b) {
                    fprintf(stderr, "Loading block %s FAILED\n", argv[0]);
                    return 1; // error return code.
                }
            }
            return 0;

        case 'S': // --parameter-set BLOCK_NAME PARAMETER_NAME VALUE
            // We must have 3 additional args:
            if(numArgs < 3) {
                fprintf(stderr, "--parameter-set without 3 args\n");
                return 1;
            }
            if(!graph) {
                // If there not graph than their are no blocks.
                fprintf(stderr, "--parameter-set "
                        "with no blocks loaded");
                return 1;
            }
            return ParameterSet(argv[0], argv[1], &argv[2], numArgs-2);

        case  'c': // --connect block0 outPort block1 inPort
            // We must have 4 additional args:
            if(numArgs != 4) {
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
            // get block0
            uint32_t fromPortNum = strtoul(argv[1], &end, 10); 
            if(argv[0] == end) {
                fprintf(stderr, "--connect bad outPort number: %s\n",
                            argv[1]);
                return 1;
            }
            uint32_t toPortNum = strtoul(argv[3], &end, 10); 
            if(argv[3] == end) {
                fprintf(stderr, "--connect bad inPort number: %s\n",
                        argv[3]);
                return 1;
            }
            struct QsBlock *b0 = qsGraphGetBlockByName(graph, argv[0]);
            if(!b0) {
                fprintf(stderr, "--connect: block \"%s\" not found\n",
                        argv[0]);
                return 1;
            }
            struct QsBlock *b1 = qsGraphGetBlockByName(graph, argv[2]);
            if(!b1) {
                fprintf(stderr, "--connect: block \"%s\" not found\n",
                        argv[2]);
                return 1;
            }
            if(qsBlockConnect(b0, b1, fromPortNum, toPortNum)) {
                fprintf(stderr, "--connect: failed to connect\n");
                return 1;    
            }
            return 0;

        case 'C': // --connect-parameters
                // blockName0 parameterName0 blockName1 parameterName1
                // We must have 4 additional args:
            if(numArgs != 4) {
                fprintf(stderr, "--connect-parameters "
                            "without 4 more args: "
                            "block0 parameter0 block1 parameter1\n");
                return 1;
            }
            if(!graph) {
                // If there not graph than their are no blocks.
                fprintf(stderr, "--connect-parameters "
                        "with no blocks loaded");
                return 1;
            }
            {
                // All these blocks and parameter names exist.

                struct QsBlock *b0 = qsGraphGetBlockByName(graph,
                        argv[0]);
                // The from parameter must not be a setter.
                struct QsParameter *p0 = b0?
                    qsParameterGetPointer(b0, argv[1],
                            false/*isSetter*/):0;

                struct QsBlock *b1 = qsGraphGetBlockByName(graph,
                        argv[2]);
                // If p0 is a constant parameter than p1 could be a
                // setter or a constant.
                struct QsParameter *p1 = b1?
                    qsParameterGetPointer(b1, argv[3],
                            /*isSetter*/true):0;
                if(b1 && !p1 && qsParameterGetKind(p0) == QsConstant)
                    // Okay maybe it's a constant
                    p1 = qsParameterGetPointer(b1, argv[3],
                            /*isSetter*/false);

                if(!b0 || !p0 || !b1 || !p1) {
                    fprintf(stderr, "--connect-parameters "
                            "%s %s %s %s FAILED: ",
                            argv[0], argv[1],
                            argv[2], argv[3]);
                    if(!b0)
                        fprintf(stderr, "Failed to find "
                                "block named \"%s\" ",
                                argv[0]);
                    else if(!p0)
                        fprintf(stderr, "Failed to find "
                                "parameter named \"%s\" in block "
                                "\"%s\" ",
                                argv[1], argv[0]);
                    if(!b1)
                        fprintf(stderr, "Failed to find "
                                "block named \"%s\" ",
                                argv[2]);
                    else if(!p1)
                        fprintf(stderr, "Failed to find "
                                "parameter named \"%s\" in block "
                                "\"%s\" ",
                                argv[3], argv[2]);
                    fprintf(stderr,"\n");
                    return 1;
                }

                if(qsParameterConnect(p0, p1))
                    return 1;
                return 0;
            }

        case 's': // sleep SECONDS
            if(numArgs != 1) {
                fprintf(stderr, "--sleep with bad SECONDS\n");
                return 1;
            }
            {
                errno = 0;
                char *endptr = 0;
                double val = strtod(argv[0], &endptr);
                if(endptr == argv[0] || val <= 0.0) {
                    fprintf(stderr, "Bad --sleep option\n\n");
                    return 1;
                }
                if(level >= 4) // 4 = INFO
                    fprintf(stderr,"Sleeping %lg seconds\n", val);
                usleep( (useconds_t) (val * 1000000.0));
                return 0;
            }

        case 'P': // --parameters-print
            if(graph)
                qsGraphParametersPrint(graph, stdout);
            else
                fprintf(stdout, "\n");
            return 0;

        case 'i': // --interpreter
        {
            const char *arg = 0;
            if(numArgs)
                arg = argv[0];
            return RunInterpreter(arg);
        }

        case 'd': // --display Generate a graphviz dot graph and run the
            // imagemagick display program and continue running
            if(graph)
                qsGraphPrintDotDisplay(graph, false/*waitForDisplay*/);
            return 0;

        case 'D': // --display-wait Generate a graphviz dot graph and run the
            // imagemagick display program and wait for the display
            // program to exit and then continue running
            if(graph)
                qsGraphPrintDotDisplay(graph, true/*waitForDisplay*/);
            return 0;

        case 'o': // --dot  Print a graphviz dot file of the stream graph
            // to stdout
            if(graph)
                qsGraphPrintDot(graph, stdout);
            else
                fprintf(stdout, "\n");
            return 0;

        case 'g': // --graph
            CreateGraph();
            return 0;
        
        case 'r': // --run run all the stream flow graphs
        {
            // TODO: We need to explore combinations of failure modes
            // for this flow/run sequence of calls.  Can we cleanup
            // while the stream is flowing?  What about when there is
            // more than one graph and only some graphs fail?
            for(uint32_t i = 0; i<numGraphs; ++i)
                if(qsGraphRun(graphs[i]))
                    return 1;
            //
            // Now wait for them to finish running:
            for(uint32_t i = 0; i<numGraphs; ++i)
                // For the case when there are no other threads than this
                // main thread this will not block and do the right thing.
                if(qsGraphWait(graphs[i]))
                    return 1;
            return 0;

        case 't': // --threads
            if(numArgs != 1) {
                fprintf(stderr, "--threads with bad MAX_THREADS\n");
                return 1;
            }
            {
                errno = 0;
                char *endptr = 0;
                uint32_t maxThreads = strtol(argv[0], &endptr, 10);
                if(endptr == argv[0] || maxThreads > 1000000) {
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
                return 0;
            }

            case 'v': // --verbose
                if(numArgs != 1) {
                    fprintf(stderr, "--verbose with bad level\n");
                    return 1;
                }
                
                // LEVEL maybe debug, info, notice, warn, error, and
                // off which translates to: 5, 4, 3, 2, 1, and 0
                // as this program (and not the API) define it.
                if(*argv[0] == 'd' || *argv[0] == 'D' || *argv[0] == '5')
                    // DEBUG 5
                    level = 5;
                else if(*argv[0] == 'i' || *argv[0] == 'I' || *argv[0] == '4')
                    // INFO 4
                    level = 4;
                else if(*argv[0] == 'n' || *argv[0] == 'N' || *argv[0] == '3')
                    // NOTICE 3
                    level = 3;
                else if(*argv[0] == 'w' || *argv[0] == 'W' || *argv[0] == '2')
                    // WARN 2
                    level = 2;
                else if(*argv[0] == 'e' || *argv[0] == 'E' || *argv[0] == '1')
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
                return 0;

            default:
            // This should not happen, unless the options are not coded
            // correctly.  We are missing a case for this char.  This
            // should be a case that is caught in case '*'.  That means we
            // have a short option char that is not in this switch().
            ERROR("BAD CODE: Missing case for option character case: "
                        "%c opt=%s arg=%s\n", comm, argv[1], argv[0]);
            fprintf(stderr, "unknown option character: %c\n", comm);
            // This will exit with error.  The program need more
            // coding.  We are totally screwed.
            return 0;
        }
    }

    return 0;
}
