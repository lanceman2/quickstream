#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "../lib/debug.h"

#include "../include/quickstream.h"

#include "./quickstream.h"

// qsOptions.h is a generated file that defines options for this program.
#include "../lib/quickstream/misc/qsOptions.h"

#include "../lib/parseBool.h"


// The "current" graph:
struct QsGraph *graph = 0;


static inline void
GetDefaultGraph(void) {
    graph = qsGraph_create(0, DEFAULT_MAXTHREADS,
            0/*name*/, 0/*thread pool name*/,
            QS_GRAPH_IS_MASTER | QS_GRAPH_SAVE_ATTRIBUTES);
    ASSERT(graph);
}


static inline
void SPEW(int c, int argc, const char *command,
        const char * const * argv) {

    // When there are other threads printing we do not want the text
    // inter-mixed, so we make a string and then print to stderr.
    size_t len = 256; // guessing this "len" is long enough.
    char str[len];
    char *s = str;
    int w = snprintf(s, len, "Running Command: %s[%c]:", command, c);
    if(w < len) {
        len -= w;
        s += w;
        for(int i=0;i<argc;++i) {
            w = snprintf(s, len, " %s", argv[i]);
            if(w >= len)
                break;
            len -= w;
            s += w;
        }
    }
    fprintf(stderr, "%s\n", str);
}


// To make all the error spew look similar and shorten the code.
//
static inline bool ErrorRet(int ret,
        int argc, const char * const * argv, const char *command,
        const char *fmt, ...) {

    exitStatus = ret;
    // We wish to keep all the spew in one string so that we do not get
    // other threads inter-sprucing text in-between a few prints to stderr.
    const size_t LEN = 512;
    char buf[LEN];
    int len = snprintf(buf, LEN, "FAILED \"%s\" command: %s",
            command, argv[0]);
    for(int i=1; i<argc && len > 0 && len < LEN; ++i) {
        int r = snprintf(buf + len, LEN - len, " %s", argv[i]);
        ASSERT(r >= 0);
        len += r;
    }
    if(len <= 0 || len >= LEN-3) {
        // This should not happen.
        DASSERT(0);
        fputs(buf, stderr);
        return true;
    }

    int r = snprintf(buf + len, LEN - len, ": ");
    ASSERT(r >= 0);
    len += r;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + len, LEN - len, fmt, ap);
    va_end(ap);
    fputs(buf, stderr);
    return true;
}


// Returns true on failure.
static inline bool Strtod(const char *s, double *ret) {

    DASSERT(s);

    char *end = 0;

    double val = strtod(s, &end);
    if(end == s)
        // fail
        return true;
    *ret = val;
    return false;
}


static pthread_t masterThread;
static int sig_num = 0;

static void Catcher(int sig) {
    //DSPEW("Caught signal %d", sig);

    if(!pthread_equal(pthread_self(), masterThread))
        pthread_kill(masterThread, sig);
}



//
// c       is the command as a single character from
//         ../lib/quickstream/misc/qsOptions.h.  The short option form of
//         the command.  Used to switch(c){}.
//
// argc    is the argument count that in includes the first argument which
//         is the command argument, argv[0].  This is argc from
//         main(argc, argv) but is less than the main argc.
//
// argv[0] is the first argument that determined the command
//         to run.  It could have been a short command-line option like -h
//         or h, or a long command option like --help or help.
//         This argv points into the argv that was passed to main(), and
//         is not the same as the argv passed to main().
//
// argv[1  to argc-1] is the rest of the arguments that are option
//         arguments for the command.
//
// command is the canonical name of the command as we define it; for
//         example: "help" could have been from: -h, h, --help, help
//
// exitStatus is set if this returns true and the status is non-zero.
//
//         Error numbers:  TODO: Go through and fix up error numbers.
//
//            The return status only keeps the first 8 bits, even though
//            the return type is an int for main().
//          I tried to compile:
//
//          unsigned char main(void) {
//                return 255;
//           }
//
//           but that fails to compile.  I think only int main() with
//           compile without an error.
//  returnStatus.c:6:15: error: return type of ‘main’ is not ‘int’ [-Werror=main]
//           but without the -Werror cc flag it compiles and runs.
//
//            so it's from 0 to 255 ; so return 256; will return
//            the status of 0.  Tests show this to be the case on
//            a Ubuntu version 20.04, x86_64 GNU/Linux 64 bit system
//            with GCC version 9.4.0.
//
//
// Any extra arguments are ignored.
//
// Returns false on success and keep calling this, or true in the case we
//         are done running and the exitStatus will be set if it is
//         non-zero.
//
//      exitOnError is uses in the command-line argument running in
//      quickstream.c and in 
//
bool RunCommand(int c, int argc, const char *command,
        const char * const * argv) {

    if(spewLevel >= 5)
        SPEW(c, argc, command, argv);


    DASSERT(argc >= 1);


    switch(c) {


        case ADD_METADATA_MK: // --add-metadata-mk MK KEY ARG0 [ARGS...] MK
            {
                if(!graph)
                    return ErrorRet(2, argc, argv, command,
                         "graph not set\n");

                if(argc < 5)
                     return ErrorRet(2, argc, argv, command,
                                "missing option arguments\n");
                if(strcmp(argv[1], argv[argc-1]))
                    return ErrorRet(2, argc, argv, command,
                                "marker arguments do not match:"
                                " \"%s\" != \"%s\"\n",
                                argv[1], argv[argc-1]);

                size_t size = 1;
                for(int i=3; i<argc-1; ++i)
                    size += strlen(argv[i]) + 1;
                char *data = calloc(1, size);
                ASSERT(data, "calloc(1,%zu) failed", size);
                char *s = data;

                for(int i=3; i<argc-1; ++i) {
                    strcpy(s, argv[i]);
                    s += strlen(argv[i]) + 1;
                }

                // This will copy the data to new memory.
                int ret = qsGraph_setMetaData(graph, argv[2], data, size);
#ifdef DEBUG
                memset(data, 0, size);
#endif
                free(data);

                if(ret)
                    return ErrorRet(2, argc, argv, command,
                                "key \"%s\" in use already\n", argv[2]);
            }
            break;


        case BLOCK: // --block FILENAME [NAME]
            if(argc < 2)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph) GetDefaultGraph();
            {
                struct QsBlock *b;
                if(argc >= 3)
                    b = qsGraph_createBlock(graph, 0,
                            argv[1], argv[2], 0);
                else
                    b = qsGraph_createBlock(graph, 0,
                            argv[1], 0, 0);
                if(!b)
                    return ErrorRet(2, argc, argv, command,
                         "cannot load block\n");
            }
            break;


        case BLOCK_UNLOAD: // --block-unload NAME [NAME ...]
            if(argc < 2)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            for(int i = 1; i < argc; ++i) {
                struct QsBlock *b = qsGraph_getBlock(graph, argv[i]);
                if(!b && exitOnError)
                    return ErrorRet(2, argc, argv, command,
                         "block \"%s\" not found\n", argv[i]);
                if(!b) continue;
                int ret = qsBlock_destroy(b);
                if(ret < 0 && exitOnError)
                    return ErrorRet(2, argc, argv, command,
                         "block \"%s\" undeclare returned %d\n", argv[i],
                         ret);
            }
            break;


        case CATCH_SIG: // --catch-sig [SIG_NUM]
            {
                sig_num = SIGINT; // SIGINT = 2
                if(argc >= 2) {
                    char *end = 0;
                    sig_num = strtol(argv[1], &end, 10);
                    if(end == argv[1] || sig_num <= 0) {
                        sig_num = 0;
                        return ErrorRet(2, argc, argv, command,
                            "bad SIG_NUM argument \"%s\"\n", argv[1]);
                    }
                }
                masterThread = pthread_self();
                struct sigaction act;
                memset(&act, 0, sizeof(act));
                act.sa_handler = Catcher;
                act.sa_flags = SA_RESETHAND;
                ASSERT(0 == sigaction(sig_num, &act, 0));
            }
            break;


        case CONFIGURE_MK:
            // --configure-mk MK BLOCK_NAME ATTR_NAME ARG1 ARG2 ... MK
            if(argc < 5)
                return ErrorRet(2, argc, argv, command,
                         "bad usage argc=%d\n", argc);
            {
                struct QsBlock *b = qsGraph_getBlock(graph, argv[2]);
                if(!b && exitOnError)
                    return ErrorRet(2, argc, argv, command,
                         "block \"%s\" not found\n", argv[2]);
                if(qsBlock_config(b, argc-4, argv+3) && exitOnError)
                    return ErrorRet(2, argc, argv, command, "failed\n");
            }
            break;


        case CONNECT:
            // --connect BLOCK_A TYPE_A PORT_A BLOCK_B TYPE_B PORT_B
            if(argc < 7)
                return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            // TODO: We may want to deal with the return positive return
            // values of qsGraph_connectByString() which is when the
            // connection alread exists.
            if(qsGraph_connectByStrings(graph,
                        argv[1], argv[2], argv[3],
                        argv[4], argv[5], argv[6]) &&
                    exitOnError)
                return ErrorRet(2, argc, argv, command, "failed\n");
            break;


        case DISCONNECT: // --dusconnect BLOCK PORT_TYPE PORT
            if(argc < 4)
                return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            if(qsGraph_disconnectByStrings(graph,
                        argv[1], argv[2], argv[3]) &&
                    exitOnError)
                return ErrorRet(2, argc, argv, command, "failed\n");
            break;


        case DISPLAY: // --display
            // Display a vizgraph dot file of the graphs.
            //
            qsGraph_printDotDisplay(0/*graph*/,
                        false/*waitForDisplay*/,
                        0/*optionsMask*/);
            break;


        case DISPLAY_WAIT: // --display-wait
            // Display a vizgraph dot file of the graph.
            //
            qsGraph_printDotDisplay(0,
                        true/*waitForDisplay*/,
                        0/*optionsMask*/);
            break;


        case DOT: // --dot
            // print a vizgraph dot file of the graphs.
            //
            qsGraph_printDot(0/*graph*/, stdout, 0/*opts*/);
            break;


        case EXIT_ON_ERROR: // --exit-on-error [True|False]
            //
            if(argc < 2)
                exitOnError = true;
            else
                exitOnError = qsParseBool(argv[1]);
            break;


        case GRAPH:
            // --graph [NAME [MAXTHREADS [TP_NAME [SUPERBLOCK [HALT]]]]]
            {
                const char *gname   = 0;
                uint32_t maxThreads = DEFAULT_MAXTHREADS;
                const char *tpname  = 0;
                const char *sbfile  = 0;
                uint32_t halt = 0;
                if(argc >= 2)
                    gname = argv[1];
                if(argc >= 3) {
                     maxThreads = strtoul(argv[2], 0, 10);
                     if(maxThreads < 1)
                         maxThreads = DEFAULT_MAXTHREADS;
                }
                if(argc >= 4)
                    tpname = argv[3];
                if(argc >= 5)
                    sbfile = argv[4];
                if(argc >= 6)
                    halt = qsParseBool(argv[5])?QS_GRAPH_HALTED:0;
                struct QsGraph *g = qs_getGraph(gname);
                if(!g) g = qsGraph_create(sbfile, maxThreads,
                        gname, tpname,
                        QS_GRAPH_IS_MASTER |
                        QS_GRAPH_SAVE_ATTRIBUTES | halt/*flags*/);
                if(!g) return ErrorRet(2, argc, argv, command,
                        "failed\n");
                // Set the "current graph".
                graph = g;
            }
            break;


        case HALT: // --halt
            if(!graph)
                graph = qsGraph_create(0, DEFAULT_MAXTHREADS, 0, 0,
                        QS_GRAPH_IS_MASTER | QS_GRAPH_SAVE_ATTRIBUTES);
            qsGraph_halt(graph);
            break;


        case HELP: // --help
            //
            // Print help to stdout and then exit 1
            help(STDOUT_FILENO, "-H"); // This does not return.


        case INTERPRETER: // --interpreter [FILE [LINENO]]
            //
            // Run the interpreter from here on out.
            if((exitStatus = RunInterpreter(argc-1, argv+1)))
                return ErrorRet(2,
                            argc, argv, command,
                            "quickstream Interpreter failed\n");
            break;


        case MAKE_PORT_ALIAS:
            // --make-port-alias
            //    BLOCK_NAME PORT_TYPE PORT_NAME ALIAS_PORT_NAME
            //
            if(argc < 5)
                return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            if(qsBlock_makePortAlias(graph, argv[1], argv[2],
                        argv[3], argv[4])
                    && exitOnError)
                    return ErrorRet(2, argc, argv, command, "\n");
            break;


        case PARAMETER_SET_MK:
            // --parameter-set-mk
            // MK BLOCK_NAME PARAMETER_NAME [VALUE ...] MK
            //
            // Set a control parameter value for a setter that is not
            // connected yet.
            if(argc < 5)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            {
                struct QsBlock *b = qsGraph_getBlock(graph, argv[2]);
                if(!b && exitOnError)
                    return ErrorRet(2, argc, argv, command,
                         "block \"%s\" not found\n", argv[2]);
                if(!b) break;
                struct QsParameter *p = qsBlock_getSetter(
                        b, argv[3]);
                if(!p && exitOnError)
                    return ErrorRet(2, argc, argv, command,
                         "parameter \"%s:%s\" not found\n", argv[2],
                         argv[3]);
                int ret = qsParameter_setValueByString(p, argc-5, argv + 4);
                if(ret && exitOnError)
                    return ErrorRet(2, argc, argv, command,
                         "parameter \"%s:%s\" cannot set\n", argv[2],
                         argv[3]);
            }
            break;


        case PRINT_METADATA: // --print-metadata KEY
            //
            // This is more of just a test and not so useful for users.
            //
            if(argc < 2)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");

            {
                size_t len = 0;
                char *str = qsGraph_getMetaData(graph, argv[1], &len);
                if(!str)
                    return ErrorRet(2, argc, argv, command,
                         "metadata at key \"%s\" not found\n", argv[1]);
                if(len > 2) {
                    char *x = str;
                    // This may be a crappy thing to print as a string but
                    // we'll at least not let the program segfault but
                    // double 0 terminating the stringy thingy as it was
                    // supposed to be created as in --add-metadata-mk.
                    x[len-1] = '\0';
                    x[len-2] = '\0';
                    while(*x) {
                        printf(" \"%s\"", x);
                        x += strlen(x) + 1;
                    }
                }
                free(str);
                printf("\n");
            }
            break;


        case RENAME_BLOCK: // --rename-block BLOCK_NAME NEW_NAME
            //
            if(argc < 3)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            {
                struct QsBlock *b = qsGraph_getBlock(graph, argv[1]);
                if(!b)
                    return ErrorRet(2, argc, argv, command,
                             "block \"%s\" not found\n", argv[1]);
                if(qsBlock_rename(b, argv[2]))
                    return ErrorRet(2, argc, argv, command,
                             "block rename failed\n");
            }
            break;


        case SAVE: // --save BLOCK_FILENAME [GRAPH_FILENAME]
            //
            if(argc < 2)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            {
                const char *gfile = 0;
                if(argc > 2)
                    gfile = argv[2];
                if(qsGraph_save(graph, argv[1], gfile, 0))
                    return ErrorRet(2, argc, argv, command,
                             "save failed\n");
            }
            break;


        case SAVE_BLOCK: // --save-block PATH
            if(argc < 2)
                 return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            if(qsGraph_saveSuperBlock(graph, argv[1], 0))
                return ErrorRet(2, argc, argv, command,
                         "saving to \"%s\" failed\n", argv[1]);
            break;


        case SAVE_CONFIG: // --save-config [ON]

            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            {
                bool on = true;
                if(argc > 1)
                    on = qsParseBool(argv[1]);
                qsGraph_saveConfig(graph, on);
            }
            break;


        case SET_BLOCK_PATH: // --set-block-path [PATH]

            if(argc <= 1)
                ASSERT(unsetenv("QS_BLOCK_PATH") == 0);
            else
                ASSERT(setenv("QS_BLOCK_PATH", argv[1], 1) == 0);
            break;


        case SLEEP: // --sleep [SEC]
            //
            {
                struct timespec t = { 0, 0 };
                if(argc >= 2) {
                    char *end = 0;
                    double sec = strtod(argv[1], &end);
                    if(end == argv[1] || sec < 0.0)
                        return ErrorRet(2, argc, argv, command,
                                "bad SEC argument \"%s\"\n", argv[2]);
                    t.tv_sec = sec;
                    t.tv_nsec = (sec - t.tv_sec) * 1000000000.0;
                    if(t.tv_nsec < 0 || t.tv_nsec > 999999999)
                        t.tv_nsec = 999999999;
                }
                if(t.tv_sec == 0 && t.tv_nsec == 0)
                    pause();
                else
                    nanosleep(&t, 0);

                if(sig_num) {
                    // Reset the signal handler.  It may be reset already
                    // from when a signal was catch but resetting it again
                    // will not hurt.
                    struct sigaction act;
                    memset(&act, 0, sizeof(act));
                    act.sa_handler = SIG_DFL;
                    ASSERT(0 == sigaction(sig_num, &act, 0));
                    sig_num = 0;
                }
            }
            break;


        case START: // --start
            //
            // Run the stream for the current graph
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "no current graph set\n");
            if(qsGraph_start(graph) && exitOnError)
                return ErrorRet(2, argc, argv, command,
                        "failed\n");
            break;


        case STOP: // --stop
            //
            // Stop the stream for the current graph
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "graph not set\n");
            if(qsGraph_stop(graph) && exitOnError)
                return ErrorRet(2, argc, argv, command,
                        "failed\n");
            break;


        case THREADS:// --threads MAX_THREADS [TP_NAME]

            if(argc < 2)
                return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            {
                char *end = 0;
                const char *tpname = 0;
                if(argc >= 3)
                    tpname = argv[2];
                uint32_t maxThreads = strtoul(argv[1], &end, 10);
                if(end == argv[1] || maxThreads == 0)
                    return ErrorRet(2, argc, argv, command,
                         "bad NUM argument\n");
                if(!graph) {
                    graph = qsGraph_create(0, maxThreads, 0/*graph name*/,
                            tpname,
                            QS_GRAPH_IS_MASTER | QS_GRAPH_SAVE_ATTRIBUTES);
                    ASSERT(graph);
                    break;
                }
                struct QsThreadPool *tp = 0;
                if(tpname)
                    tp = qsGraph_getThreadPool(graph, tpname);
                if(!tp)
                    // A newly created thread pool is the default
                    // thread pool.
                    tp = qsGraph_createThreadPool(graph,
                            maxThreads, tpname);
                else {
                    qsGraph_setDefaultThreadPool(graph, tp);
                    qsThreadPool_setMaxThreads(tp, maxThreads);
                }
            }
            break;


        case THREADS_ADD:// --threads-add TP_NAME BLOCK0 [BLOCK1 ...]

            if(argc < 3)
                return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "no graph exists\n");
            {
                const char * const *blockName = argv + 2;
                const char * const *end = argv + argc;
                struct QsThreadPool *tp =
                    qsGraph_getThreadPool(graph, argv[1]);
                if(!tp)
                    return ErrorRet(2, argc, argv, command,
                         "thread pool %s not found\n", argv[1]);
                for(;blockName != end; ++blockName) {
                    struct QsBlock *b =
                        qsGraph_getBlock(graph, *blockName);
                    if(!b && exitOnError)
                        return ErrorRet(2, argc, argv, command,
                                "block \"%s\" not found\n", *blockName);
                    qsThreadPool_addBlock(tp, b);
                }
            }
            break;


        case THREADS_DESTROY:// --threads-destroy TP_NAME [...]

            if(argc < 2)
                return ErrorRet(2, argc, argv, command,
                         "bad usage\n");
            if(!graph)
                return ErrorRet(2, argc, argv, command,
                         "no graph exists\n");
            for(int i=1; i<argc; ++i) {
                struct QsThreadPool *tp =
                    qsGraph_getThreadPool(graph, argv[i]);
                if(!tp)
                    return ErrorRet(2, argc, argv, command,
                            "thread pool %s not found\n", argv[i]);
                if(qsGraph_getNumThreadPools(graph) < 2)
                    return ErrorRet(2, argc, argv, command,
                            "thread pool %s is the last one\n", argv[i]);
                qsThreadPool_destroy(tp);
            }
            break;


        case USAGE: // --usage
            // Print usage help, in this case, to stdout and then exit 1
            help(STDOUT_FILENO, "-u"); // This does not return.


        case UNHALT: // --unhalt
            if(graph)
                qsGraph_unhalt(graph);
            break;


        case VERSION: // --version
            //
            // Print the version to stdout and then exit 0
            printf("%s\n", QUICKSTREAM_VERSION);
            exit(0);


        case VERBOSE: // --verbose LEVEL
            if(argc < 2)
                 return ErrorRet(2, argc, argv, command,
                         "missing verbose LEVEL arg\n");
            errno = 0;
            int x = strtol(argv[1], 0, 10);
            if(errno || x > 5)
                return ErrorRet(2, argc, argv, command,
                         "missing verbose LEVEL not between 0 and 5\n");

            qsSetSpewLevel(spewLevel = x);
            if(spewLevel >= 5)
                fprintf(stderr, "Spew level set to %d\n", spewLevel);
            break;


        case WAIT: // --wait [SECONDS]
                   //
                   // TODO: What if there is more than one graph?
                   //
            {
                if(!graph)
                    // We have nothing to wait for.  And I guess that's
                    // okay.
                    break;
                double seconds = 0;
                if(argc > 1) {
                    char *end = 0;
                    seconds = strtod(argv[1], &end);
                    if(end == argv[1])
                        return ErrorRet(2, argc, argv,
                                command,
                                "bad SECONDS argument: \"%s\"\n",
                                argv[1]);
                }
                int ret = qsGraph_wait(graph, seconds);
                if(ret == 1)
                    // The graph has been destroyed.
                    graph = 0;
            }
            break;


        case WAIT_FOR_DESTROY: // --wait-for-destroy [SECONDS]
                   //
                   // TODO: What if there is more than one graph?
                   //
            {
                if(!graph)
                    // We have nothing to wait for.  And I guess that's
                    // okay.
                    break;
                double seconds = 0;
                if(argc > 1) {
                    char *end = 0;
                    seconds = strtod(argv[1], &end);
                    if(end == argv[1])
                        return ErrorRet(2, argc, argv,
                                command,
                                "bad SECONDS argument: \"%s\"\n",
                                argv[1]);
                }
                if(qsGraph_waitForDestroy(graph, seconds))
                    // The graph has been destroyed.
                    graph = 0;
            }
            break;


        case WAIT_FOR_STREAM: // --wait-for-stream [SECONDS]
                   //
                   // TODO: What if there is more than one graph?
                   //
            {
                if(!graph)
                    // We have nothing to wait for.  And I guess that's
                    // okay.
                    break;
                double seconds = 0;
                if(argc > 1) {
                    char *end = 0;
                    seconds = strtod(argv[1], &end);
                    if(end == argv[1])
                        return ErrorRet(2, argc, argv,
                                command,
                                "bad SECONDS argument: \"%s\"\n",
                                argv[1]);
                }
                if(qsGraph_waitForStream(graph, seconds))
                    // The graph has been destroyed.
                    graph = 0;
            }
            break;


        default:
            // main() should have sanitized this command before this
            // function; so that only command that are listed in the
            // lib/quickstream/misc/quickstreamHelp program are passed to
            // here; so we need to add another switch case to this
            // switch.
            DASSERT(0, "unknown command %c \"%s\": write more code"
                    " for that command or remove that command from "
                    "lib/quickstream/misc/quickstreamHelp",
                    c, argv[0]);
            return ErrorRet(101, argc, argv, command,
                         "unknown command\n");
    }

    return false; // success
}
