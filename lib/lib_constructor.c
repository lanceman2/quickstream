#define _GNU_SOURCE
#include <link.h>

#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"

#include "builtInBlocks.h"



// Default control parameter queue length:
//
// This gets set in here if env QS_PARAMETER_QUEUE_LENGTH is good.
uint32_t parameterQueueLength = ((uint32_t) 10);


// env QS_WAIT_SIGNAL can make this variable if it is good.
//
int waitSignal = 0; // The value it set to non-zero below.

sigset_t waitSigSet;


/* Key for the thread pool worker thread-specific data */
pthread_key_t threadPoolKey;

/* Key for seeing if a block calls the libquickstream.so API. */
pthread_key_t blockKey;




// It is possible that these are not set, but it's a special case where
// this is not compiled as a DSO (dynamic shared object) and the program
// that this code is statically link to is not in at path (say PATH) such
// that the directory like dirname(PATH)/../lib/
const char *qsBlockDir = 0;
const char *qsLibDir = 0;

struct QsDictionary *blocksPerProcess = 0;



static int dl_callback(struct dl_phdr_info *info,
                        size_t size, void *data) {


    //                                  "/lib/libquickstream.so"
    const char *LibName = DIRSTR "lib" DIRSTR "libquickstream.so";

    char *libDir = 0;
    size_t LibNameLen = strlen(LibName);

    // The path must include this string.  If someone changes the
    // filename, LibName, in the source and installation, this will be
    // broken; lots of things will be broken; like the ability to load
    // core block modules.
    const size_t len = strlen(info->dlpi_name);
    if(len < LibNameLen) return 0;
    const char *end = info->dlpi_name + len - LibNameLen;


    for(const char *c = info->dlpi_name; c <= end; ++c) {
        if(0 == strncmp(c, LibName, LibNameLen)) {
            libDir = realpath(info->dlpi_name, 0);
            ASSERT(libDir);
            break;
        }
    }

    if(libDir) {
        // Better be a full path.
        ASSERT(*libDir == DIRCHR);
        char *s;
        for(s = libDir + strlen(libDir) - 1; *s != DIRCHR; --s)
            *s = '\0';
        // Remove the '/'.
        *s = '\0';
        // This extra const-ifying may save us some pain in the long run.
        // If some code tries to change it, at least they get a compile
        // error.
        qsLibDir = (const char *) libDir;
        return 1; // done
    }

    return 0;
}


static inline void GetLibDir(void) {

    // TODO: Note: This is very Linux specific code.
    //

    const char *exe = "/proc/self/exe";
    char *bin = realpath(exe, 0);
    ASSERT(bin, "realpath(\"%s\",0) failed", exe);
    // remove one '/' and up to next '/'.  For example:
    //
    //    bin = "/usr/game/bin/foo"
    //
    // -> bin = "/usr/game/"
    //

    char *s = bin + strlen(bin);
    const char *Lib = "lib";

    while(s != bin && *s != '/') *s-- = '\0';
    if(s != bin) --s;
    while(s != bin && *s != '/') *s-- = '\0';
    ASSERT(*s == '/', "Bad program path from \"%s\"", exe);
    const size_t Len = strlen(bin) + strlen(Lib) + 1;
    char *LibDir = calloc(1, Len);
    ASSERT(LibDir, "calloc(1,%zu) failed", Len);
    snprintf(LibDir, Len, "%s%s", bin, Lib);
    // Example:
    //
    //      LibDir = "/usr/game/lib"
    //
    qsLibDir = realpath(LibDir, 0);

    // Now qsLibDir may be found or not.  All we can do is try.
    // We check it with ASSERT() later.

    DZMEM(LibDir, Len);
    DZMEM(bin, strlen(bin));
    free(LibDir);
    free(bin);
}


static void __attribute__ ((constructor)) qsLibrary_create(void);


static void Free(void *x) {
    free(x);
}


// Looks like this can get called more than once for the case when the
// program that is running is statically linked with
// lib/libquickstream.a.  Adding printf()'s should it.
//
static void qsLibrary_create(void) {

    // I do not know why errno keeps getting set.
    errno = 0;

    DSPEW();

    // TODO: Can't we do this "if" at compile time with a CPP macro?

    if(dl_iterate_phdr(dl_callback, 0))
        DASSERT(qsLibDir);


    char *env = getenv("QS_PARAMETER_QUEUE_LENGTH");
    if(env) {
        char *end;
        unsigned long l = strtoul(env, &end, 10);
        // TODO: Test if the parameterQueueLength value of 1 can work...
        if(end != env && l >= 1 &&
                l < 1000001/*TODO: somewhat arbitrary??*/)
            parameterQueueLength = l;
    }
    DSPEW("QS_PARAMETER_QUEUE_LENGTH is %" PRIu32, parameterQueueLength);


    env = getenv("QS_WAIT_SIGNAL");
    if(env) {
        char *end;
        long l = strtol(env, &end, 10);
        if(end != env && l >= SIGRTMIN && l <= SIGRTMAX)
            waitSignal = l;
    }
    if(!waitSignal) {
        // real-time signals are designed to be used for
        // application-defined purposes.  See "man 7 signal".
        waitSignal = SIGRTMIN + 3;
        ASSERT(waitSignal >= SIGRTMIN);
        ASSERT(waitSignal <= SIGRTMAX);
    }

    DSPEW("QS_WAIT_SIGNAL is %d", waitSignal);

    // Block that signal for now.
    ASSERT(0 == sigemptyset(&waitSigSet));
    ASSERT(0 == sigaddset(&waitSigSet, waitSignal));
    ASSERT(0 == pthread_sigmask(SIG_BLOCK, &waitSigSet, 0));


    if(!qsLibDir)
        // We need to get qsLibDir another way, if dl_iterate_phdr()
        // running dl_callback() fails to get it.  That happens if this
        // code is statically linked with libquickstream.a.
        GetLibDir();

    if(!qsLibDir)
        // It could have failed again, but we can still keep running.
        WARN("The quickstream library directory was not found");

    if(qsLibDir) {
        DSPEW("The quickstream library directory is: %s", qsLibDir);

        // We also need a good core relative block module path and we must
        // get it at program startup.


        // TODO: Do we really need to keep qsLibDir?
        //
        // Anyhow, we derive qsBlockDir from qsLibDir and we'd get the
        // value of qsLibDir along the way to getting qsLibDir.

        // To get to the blocks directory from the lib directory:
        //                    "/quickstream/blocks"
        //
        const char *BlockSuffix = DIRSTR "quickstream" DIRSTR "blocks";

        // Now compose the top core block module directory.
        const size_t len = strlen(qsLibDir) + strlen(BlockSuffix) + 1;
        qsBlockDir = calloc(1, len);
        ASSERT(qsBlockDir);
        strcpy((char *) qsBlockDir, qsLibDir);
        strcat((char *) qsBlockDir, BlockSuffix);

        DSPEW("The quickstream core block module directory is: %s",
                qsBlockDir);
    } else
        WARN("The quickstream core block module directory is not set");


    if(blocksPerProcess)
        // This function was called already.
        return;

    // This blocksPerProcess list is needed while this process is running;
    // which is the same as when this libquickstream.so library is loaded,
    // so long as this program is linked with the libquickstream.so
    // library.
    //
    GatherBuiltInBlocksFunctions();
    blocksPerProcess = qsDictionaryCreate();
    CHECK(pthread_key_create(&threadPoolKey, 0));
    CHECK(pthread_key_create(&blockKey, Free));
}



static void DestroyGraph(struct QsGraph *g) {
    // I'm guessing there are no other threads, in this process, running
    // so we call the unprotected version of the graph destroyer.
    _qsGraph_destroy(g);
}



static
int SetupDestroyGraph(const char *name, struct QsGraph *g,
            void *userData) {

    DASSERT(graphs);
    // We cannot edit the dictionary as we loop through it so
    // we setup FreeValueOnDestroy callback for each entry
    // in the graphs dictionary.
    //
    struct QsDictionary *dict = qsDictionaryFindDict(graphs, name, 0);
    ASSERT(dict);
    qsDictionarySetFreeValueOnDestroy(dict,
        (void (*)(void *)) DestroyGraph);


    return 0;
}



static void __attribute__ ((destructor)) qsLibrary_destroy(void);

static void qsLibrary_destroy(void) {

    // Note: If the graphs that are created use memory from the stack in
    // main() then the cleanup of the graph object may crash the program
    // by accessing that memory after it along with the function call
    // stack is "unallocated".
    //
    // So long as the user calls qsGraph_destroy() for all the graphs they
    // create there is no problem.

    DSPEW();


    CHECK(pthread_mutex_lock(&gmutex));

    if(numGraphs) {
        WARN("Automatically destroying %" PRIu32 " remaining graphs",
                numGraphs);
        ASSERT(qsDictionaryForEach(graphs,
                    (int (*) (const char *, void *, void *))
                     SetupDestroyGraph, graphs));
        qsDictionaryDestroy(graphs);
        numGraphs = 0;
        graphs = 0;
    }

    CHECK(pthread_mutex_unlock(&gmutex));


    if(blocksPerProcess) {
        qsDictionaryDestroy(blocksPerProcess);
        blocksPerProcess = 0;

        void *x = pthread_getspecific(blockKey);
        if(x) free(x);

        CHECK(pthread_key_delete(blockKey));
        CHECK(pthread_key_delete(threadPoolKey));
        DASSERT(builtInBlocksFunctions);
        qsDictionaryDestroy(builtInBlocksFunctions);
        builtInBlocksFunctions = 0;
    }


    DASSERT((qsBlockDir && qsLibDir) || (!qsBlockDir && !qsLibDir));

    if(qsBlockDir) {
        DASSERT(qsLibDir);
        DZMEM((char *) qsBlockDir, strlen(qsBlockDir));
        free((char *) qsBlockDir);
        qsBlockDir = 0;
    }

    if(qsLibDir) {
        DZMEM((char *) qsLibDir, strlen(qsLibDir));
        free((char *) qsLibDir);
        qsLibDir = 0;
    }

    DSPEW();
}
