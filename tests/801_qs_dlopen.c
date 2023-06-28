// This tests that we can use dlopen(), dlsym(), and dlclose() to
// run a quickstream application, without leaking system resources.
//
// Most libraries are not so robust like this.
//
// Run with valgrind like:
//
//    ./valgrind_run_tests 801_qs_dlopen
//

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>


#include "../lib/debug.h"



// Why does the compiler require just this pre-declaration?
//
// WTF.
struct QsThreadPool;


// TODO: A big problem with this test is that if the API changes any of
// the functions below this test could get totally hosed.
//
// Some CPP macro madness could check these function prototypes at
// build-time.

/////////////////////////////////////////////////////////////////
//    The functions we'll dlsym() to use.
/////////////////////////////////////////////////////////////////
//
static
struct QsGraph * (*qsGraph_create)(const char *path,
        uint32_t maxThreads,
        const char *name, const char *threadPoolName,
        uint32_t flags);
//
static
struct QsBlock *(*qsGraph_createBlock)(struct QsGraph *graph,
        struct QsThreadPool *tp,
        const char *path, const char *name,
        void *userData);
//
static
int (*qsGraph_connectByStrings)(struct QsGraph *graph,
        const char *blockA, const char *typeA, const char *nameA,
        const char *blockB, const char *typeB, const char *nameB);
//
static
int (*qsGraph_start)(struct QsGraph *graph);
//
static
int (*qsGraph_stop)(struct QsGraph *graph);
//
static
int (*qsGraph_waitForStream)(struct QsGraph *g, double seconds);
//
static
int (*qsBlock_configV)(struct QsBlock *block, ...);
//
/////////////////////////////////////////////////////////////////




static
void Catcher(int sig) {
    ERROR("Caught signal %d", sig);
    ASSERT(0);
}


static void *dlhandle = 0;

static void Open(void) {

    dlhandle = dlopen("../lib/libquickstream.so", RTLD_NOW);
    ASSERT(dlhandle);


    qsGraph_create = dlsym(dlhandle, "qsGraph_create");
    ASSERT(qsGraph_create);

    qsGraph_createBlock = dlsym(dlhandle, "qsGraph_createBlock");
    ASSERT(qsGraph_createBlock);

    qsGraph_connectByStrings =
        dlsym(dlhandle, "qsGraph_connectByStrings");
    ASSERT(qsGraph_connectByStrings);

    qsGraph_start = dlsym(dlhandle, "qsGraph_start");
    ASSERT(qsGraph_start);

    qsGraph_stop = dlsym(dlhandle, "qsGraph_stop");
    ASSERT(qsGraph_stop);

    qsGraph_waitForStream = dlsym(dlhandle, "qsGraph_waitForStream");
    ASSERT(qsGraph_waitForStream);

    qsBlock_configV = dlsym(dlhandle, "qsBlock_configV");
    ASSERT(qsBlock_configV);
}


static void
Close(void) {

    // This dlclose() will call the libquickstream.so library destructor
    // which cleans up all the libquickstream.so objects.
    //
    // We hope that running this with valgrind shows no resource leaks are
    // in libquickstream.so.
    //
    dlclose(dlhandle);
}



static void
Run(void) {

    struct QsGraph *g = qsGraph_create(0, 3, 0, 0, 0);
    ASSERT(g);

    // We had to use built-in block modules and could not use DSO block
    // modules, because the DSO blocks require access to libquickstream.so
    // symbols.
    //
    // A way to use DSO blocks may be to link the DSO with
    // libquickstream.so, but that is not required when the program
    // loading the DSO is linked with libquickstream.so, so we don't
    // bother, given that would create extra compiled DSO files that are
    // just for this test.
    //
    struct QsBlock *in = qsGraph_createBlock(g, 0, "file/FileIn", "in", 0);

    ASSERT(in);
    ASSERT(qsGraph_createBlock(g, 0, "file/FileOut", "out", 0));
    ASSERT(0 == qsGraph_connectByStrings(g, "in", "o", "0", "out", "i", "0"));
    ASSERT(0 == qsBlock_configV(in, "Filename", "801_qs_dlopen.c", 0)); 

    ASSERT(qsGraph_start(g) == 0);

    qsGraph_waitForStream(g, 0);

    ERROR();
}




int main(void) {


    DSPEW();

    // Do not need this env because we will use built-in blocks.
    //ASSERT(0 == setenv("QS_BLOCK_PATH", "../lib/quickstream/misc/test_blocks", 1));

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);


    for(int i=0; i<2; ++i) {
        // We tried it and succeeded with 200 loops,
        // but that takes to long for quick tests.

        Open();

        Run();

        Close();

        // Again! Again!
    }

    // See libquickstream.so does not leak system resources.
    //
    // I.e. libquickstream.so does not shit on itself like
    // many libraries do.
    //

    return 0;
}
// NOTICE:
// We just printed out this C source file along with this last line.
///////////////////////////////////////////////////////////////////////
