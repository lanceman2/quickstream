#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"
#include "name.h"

#include "c-rbtree.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"

#include "metaData.h"


// The following global data is like the "root" block node that is the
// parent of all graphs which are parent blocks.  There is only one root
// block node.  We forgo making it: static struct QsParentBlock root, or
// something like that, because it's not quite correct, this root block
// node has no thread pool associated with it like all blocks do, and
// access to these variables is simpler.
//
// Thinking about the following static data as a "root" block node is
// still a helpful exercise.
//
// gmutex is used to sync the access of the lists of graphs.
pthread_mutex_t gmutex = PTHREAD_MUTEX_INITIALIZER;
// List of all graphs in the process.
struct QsDictionary *graphs = 0;
// The number of graphs in this process.
uint32_t numGraphs = 0;



struct QsGraph *qs_getGraph(const char *name) {

    if(!name || !name[0])
        return 0;

    CHECK(pthread_mutex_lock(&gmutex));
    if(!graphs)
        graphs = qsDictionaryCreate();

    struct QsGraph *g = qsDictionaryFind(graphs, name);
    CHECK(pthread_mutex_unlock(&gmutex));

    return g;
}


// The string that is returned must not be accessed by another thread
// which does not control the name of the graph.  If a user changes the
// name of the graph, there could be a problem if there is no thread
// coordination.  This is where the idea of a master thread is needed.
//
const char *qsGraph_getName(const struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);

    // This lock is kind-of pointless.
    CHECK(pthread_mutex_lock((void *) &gmutex));

    DASSERT(g->name);
    DASSERT(g->name[0]);

    char *name = g->name;

    CHECK(pthread_mutex_unlock((void *) &gmutex));

    // Pointless since now we pass memory back to the user while the user
    // may not be holding a lock.  The user must be the "master thread"
    // defined as the thread that can edit the graph.

    return name;
}


// The string that is returned must not be accessed by another thread
// which does not control the name of the thread pool.  If a user changes
// the name of the thread pool, there could be a problem if there is no
// thread coordination.  This is where the idea of a master thread is
// needed.
//
const char *qsThreadPool_getName(const struct QsThreadPool *tp) {

    NotWorkerThread();
    DASSERT(tp);
    struct QsGraph *g = tp->graph;
    DASSERT(g);

    // This lock is kind-of pointless.
    CHECK(pthread_mutex_lock((void *) &g->mutex));

    DASSERT(tp->name);
    DASSERT(tp->name[0]);

    char *name = tp->name;

    CHECK(pthread_mutex_unlock((void *) &g->mutex));

    // Pointless since now we pass memory back to the user while the user
    // may not be holding a lock.  The user must be the "master thread"
    // defined as the thread that can edit the graph.

    return name;
}



int qsGraph_setName(struct QsGraph *g, const char *name) {

    NotWorkerThread();
    DASSERT(g);
    ASSERT(name);
    ASSERT(name[0]);


    CHECK(pthread_mutex_lock(&gmutex));

    DASSERT(g->name);
    DASSERT(g->name[0]);

    int ret = 0;

    if(qsDictionaryFind(graphs, name) ||
            qsDictionaryFind(g->blocks, name)) {
        ERROR("Graph name \"%s\" is in use", name);
        ret = -1;
    } else {
        ASSERT(0 == qsDictionaryRemove(g->blocks, g->name));
        ASSERT(0 == qsDictionaryRemove(graphs, g->name));
        ASSERT(0 == qsDictionaryInsert(g->blocks, name, g, 0));
        ASSERT(0 == qsDictionaryInsert(graphs, name, g, 0));
        DZMEM(g->name, strlen(g->name));
        free(g->name);
        g->name = strdup(name);
        ASSERT(g->name, "strdup() failed");
    }

    CHECK(pthread_mutex_unlock(&gmutex));

    return ret;
}



int qsThreadPool_setName(struct QsThreadPool *tp, const char *name) {

    NotWorkerThread();
    DASSERT(tp);
    ASSERT(name);
    ASSERT(name[0]);
    struct QsGraph *g = tp->graph;
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    DASSERT(tp->name);
    DASSERT(tp->name[0]);

    int ret = 0;

    if(qsDictionaryFind(g->threadPools, name)) {
        ERROR("Thread pool name \"%s\" is in use", name);
        ret = -1;
    } else {
        ASSERT(0 == qsDictionaryRemove(g->threadPools, tp->name));
        ASSERT(0 == qsDictionaryInsert(g->threadPools, name, tp, 0));
        DZMEM(tp->name, strlen(tp->name));
        free(tp->name);
        tp->name = strdup(name);
        ASSERT(tp->name, "strdup() failed");
    }

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}



struct QsGraph *qsGraph_create(const char *path,
        uint32_t maxThreads,
        const char *name, const char *threadPoolName,
        uint32_t flags) {

    NotWorkerThread();
    ASSERT(maxThreads > 0, "Write more code");

    // We are just checking that the thread pool name is valid.  The
    // thread pool name is guaranteed to be unique given it is the first
    // thread pool in this graph.
    threadPoolName = GetUniqueName(0/*no dictionary exists yet*/,
            0, threadPoolName, "tp");
    if(!threadPoolName)
        // Bad thread pool name.  GetUniqueName() called ERROR()
        return 0;

    // We need to free threadPoolName before we return.

    CHECK(pthread_mutex_lock(&gmutex));


    if(!graphs)
        graphs = qsDictionaryCreate();

    // check/get the name of this graph.
    name = GetUniqueName(graphs, numGraphs, name, "g");
    if(!name) {
        CHECK(pthread_mutex_unlock(&gmutex));
        return 0; // fail.
    }


    // There are no more failure returns after here.


    struct QsGraph *g = calloc(1, sizeof(*g));
    ASSERT(g, "calloc(1,%zu) failed", sizeof(*g));

    g->blocks = qsDictionaryCreate();

    // The graph will act like a block, because it has connection port
    // aliases, which we need to be able to find with it's block name with
    // the blocks dictionary and it's struct QsPortDicts ports:
    // QsGraph::ports.
    //
    qsDictionaryInsert(g->blocks, name, g, 0);

    g->ports.inputs = qsDictionaryCreate();
    g->ports.outputs = qsDictionaryCreate();
    g->ports.setters = qsDictionaryCreate();
    g->ports.getters = qsDictionaryCreate();

    g->threadPools = qsDictionaryCreate();
    ASSERT(g->threadPools);
    g->name = (char *) name;
    g->parentBlock.block.name = (char *) name;

    CHECK(pthread_cond_init(&g->cqCond, 0));

    {
        pthread_mutexattr_t at;
        CHECK(pthread_mutexattr_init(&at));
        CHECK(pthread_mutexattr_settype(&at,
                    PTHREAD_MUTEX_RECURSIVE_NP));
        CHECK(pthread_mutex_init(&g->mutex, &at));
        CHECK(pthread_mutex_init(&g->cqMutex, &at));
        CHECK(pthread_mutexattr_destroy(&at));
    }

    struct QsThreadPool *tp =
        _qsGraph_createThreadPool(g, maxThreads, threadPoolName);
    ASSERT(tp);
    DZMEM((char *) threadPoolName, strlen(threadPoolName));
    free((char *) threadPoolName);


    // We cannot add the graph::parentBlock to the thread pool with
    // qsThreadPool_addBlock() because the graph manages the thread pools.
    // The graph::parentBlock is special.  We are boot strapping the graph's
    // event management system in this function.

    // This is a parent block without a parent which we define as the graph
    // top block.
    g->parentBlock.block.type = QsBlockType_top;
    // This graph is a block in it's own graph.
    g->parentBlock.block.graph = g;

    // Add this graph to the list of all graphs in the process.
    ASSERT(qsDictionaryInsert(graphs, name, g, 0) == 0);
    ++numGraphs;

    // For the case when the graph creates the first thread pool via
    // _qsGraph_createThreadPool() we must launch the first worker thread
    // in that thread pool.  This is bootstrapping the graph and thread
    // pool, the first thread pool and the graph are interdependent.
    //
    LaunchWorker(tp);

    CHECK(pthread_mutex_unlock(&gmutex));


    if(QS_GRAPH_HALTED & flags)
        // We needed to do this before loading a block.
        qsGraph_halt(g);

    if(QS_GRAPH_SAVE_ATTRIBUTES & flags)
        g->saveAttributes = true;

    if(path) {
        // Load block.
        struct QsBlock *b = qsGraph_createBlock(g, 0, path, 0, 0);
        if(!b) {
            ERROR("Loading block from path \"%s\" failed",
                    path);
            qsGraph_destroy(g);
            return 0;
        }
        if(b->type == QsBlockType_super) {
            qsGraph_createMetaDataDict(g, b);
            qsGraph_flatten(g);
        }
    }

    if(QS_GRAPH_IS_MASTER & flags) {
        g->haveGraphLock = true;
        CHECK(pthread_mutex_lock(&g->mutex));
    }

    return g;
}


static inline
void DestroyBlock(struct QsBlock *b) {

    switch(b->type) {
        case QsBlockType_simple:
        case QsBlockType_super:
            qsBlock_destroy(b);
            break;
        case QsBlockType_jobs:
            qsBlock_cleanup(b);
            break;
        default:
            ASSERT(0);
    }
}


// This also called by the library destructor in lib_constructor.c
//
void _qsGraph_destroy(struct QsGraph *g) {

    DASSERT(g);


    CHECK(pthread_mutex_lock(&g->mutex));

    if(g->isHalted) {
        // We have a user requested graph wide thread pool halt.
        qsGraph_unhalt(g);
        // And now we don't
        DASSERT(!g->isHalted);
    }

    if(g->haveGraphLock) {
        // We can't have the graph mutex lock while we check and stop
        // the graph runner thread.
        //
        // TODO: This is due to a runner/user being sloppy and not
        // calling qsGraph_unlock() before this.  But, maybe that's
        // okay.
        CHECK(pthread_mutex_unlock(&g->mutex));
        g->haveGraphLock =false;
    }

    // This should release this recursive mutex for real.  We need it
    // unlocked now so we can check and kill the graph runner thread, if
    // there is one.
    CHECK(pthread_mutex_unlock(&g->mutex));


    CHECK(pthread_mutex_lock(&g->cqMutex));

    g->destroyingGraph = true;

    // Remove any graph command functions that did not get called.
    //
    // It's very unlikely there are any, but it is possible there are
    // some.
    FreeGraphCommands(g);

    if(g->waiting) {
        DSPEW("Signaling the qsGraph_wait() thread from thread");
        CHECK(pthread_cond_signal(&g->cqCond));
    }

    if(g->haveGraphRunner) {
        CHECK(pthread_mutex_unlock(&g->cqMutex));

        if(!pthread_equal(pthread_self(), g->graphRunnerThread))
            CHECK(pthread_join(g->graphRunnerThread, 0));
        else
            // We cannot join a thread from the thread we are joining.
            //
            // pthread_detach() says it will cleanup all the thread
            // resources, but we do not know when.  With pthread_join() we
            // know the thread resources are cleaned up after
            // pthread_join() returns.
            CHECK(pthread_detach(g->graphRunnerThread));
    } else {
        // This will make it so that a graph runner never runs, and we
        // know it is not running because we see g->haveGraphRunner was
        // not set, and we have the g->cqMutex lock now.
        g->haveGraphRunner = true;
        CHECK(pthread_mutex_unlock(&g->cqMutex));
    }


    // Halt all thread pools in this graph:
    qsGraph_threadPoolHaltLock(g, 0);
    // That halt gave us a g->mutex lock again too.

    DASSERT(g->name);
    DSPEW("Destroying graph \"%s\"", g->name);


    // Cleanup this block top block part of this graph,
    // which will cleanup all child blocks.
    while(g->parentBlock.lastChild)
        // This can recurse.
        DestroyBlock(g->parentBlock.lastChild);


    qsGraph_threadPoolHaltUnlock(g);

    // We have no blocks and therefore there can be no jobs and
    // therefore no worker threads working.

    while(g->threadPoolStack)
        _qsThreadPool_destroy(g->threadPoolStack);


    CleanupQsGetMemory(g);

    // We did not stop the possibility of blocks doing stuff since
    // the last FreeGraphCommands(g).
    //
    // TODO: Remove the FreeGraphCommands() above???
    //
    FreeGraphCommands(g);


    DASSERT(g->threadPools == 0);
    DASSERT(g->numThreadPools == 0);


    ASSERT(0 == qsDictionaryRemove(g->blocks, g->name));
    qsDictionaryDestroy(g->blocks);
    qsDictionaryDestroy(g->ports.inputs);
    qsDictionaryDestroy(g->ports.outputs);
    qsDictionaryDestroy(g->ports.setters);
    qsDictionaryDestroy(g->ports.getters);

    if(g->metaData)
        qsDictionaryDestroy(g->metaData);


    CHECK(pthread_mutex_destroy(&g->mutex));
    CHECK(pthread_mutex_destroy(&g->cqMutex));
    CHECK(pthread_cond_destroy(&g->cqCond));


    DZMEM(g->name, strlen(g->name));
    free(g->name);

    DZMEM(g, sizeof(*g));
    free(g);
}


void qsGraph_destroy(struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);

    CHECK(pthread_mutex_lock(&gmutex));

    DASSERT(g->name);
    DASSERT(graphs);
    DASSERT(numGraphs);


    if(g->runningStreams)
        qsGraph_stop(g);


    ASSERT(qsDictionaryRemove(graphs, g->name) == 0);
    --numGraphs;

    if(numGraphs == 0) {
        qsDictionaryDestroy(graphs);
        graphs = 0;
    }

    _qsGraph_destroy(g);

    CHECK(pthread_mutex_unlock(&gmutex));
}


// making a recursive lock into a non-recursive lock.
void qsGraph_lock(struct QsGraph *g) {

    NotWorkerThread();
    ASSERT(g);

    if(g->haveGraphLock) return;

    CHECK(pthread_mutex_lock(&g->mutex));

    g->haveGraphLock = 1;
}


// making a recursive lock into a non-recursive lock.
void qsGraph_unlock(struct QsGraph *g) {

    NotWorkerThread();
    ASSERT(g);

    if(!g->haveGraphLock) return;

    CHECK(pthread_mutex_unlock(&g->mutex));

    g->haveGraphLock = 0;
}


uint32_t qsGraph_getNumThreadPools(struct QsGraph *g) {

    NotWorkerThread();
    ASSERT(g);
    CHECK(pthread_mutex_lock(&g->mutex));

    uint32_t num = g->numThreadPools;

    CHECK(pthread_mutex_unlock(&g->mutex));

    return num;
}

// This is never called internally to libquickstream.so.
void qsGraph_halt(struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);
    CHECK(pthread_mutex_lock(&g->mutex));

    if(!g->isHalted) {

        qsGraph_threadPoolHaltLock(g, 0);
        g->isHalted = true;
        if(g->feedback)
            g->feedback(g, QsHalt, g->fbData);
    }

    CHECK(pthread_mutex_unlock(&g->mutex));
}


// This is never called internally to libquickstream.so.
void qsGraph_unhalt(struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    if(g->isHalted) {

        // In the time that this g->isHalted was set any thread pools
        // created would have added an extra halt lock just after it was
        // created, so that it was always halted and never ran any worker
        // jobs.
        //
        // We do not need to worry about being in a libquickstream.so API
        // function because qsGraph_halt() and qsGraph_unhalt() is never
        // called in libquickstream.so API functions, except
        // qsGraph_create().  The calls to
        // qsGraph_threadPoolHaltLock() and qsGraph_threadPoolHaltUnlock()
        // are always paired if the g->isHalted is unset.  Here is were we
        // zero g->haltCount, and bring "balance" back to the graph.:
        //
        // Well not to me right now as I write this, but I know that in
        // the future, coming back and looking at this it will seem
        // strange.
        //
        DASSERT(g->haltCount);

        while(g->haltCount)
            qsGraph_threadPoolHaltUnlock(g);

        g->isHalted = false;
        if(g->feedback)
            g->feedback(g, QsUnhalt, g->fbData);
    }

    CHECK(pthread_mutex_unlock(&g->mutex));
}


bool qsGraph_isHalted(struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);

    // NOTE: This mutex lock makes a little sense:
    //
    // The user really needs the lock while accessing the value returned,
    // so they need a lock after calling this.  So they need to call
    // qsGraph_lock(g) before calling this, and qsGraph_unlock(g) after
    // this call and all other graph read/write/edit-like calls.
    //
    // But technically this is correct code, given we have a recursive
    // g->mutex and access of the isHalted is protected, but the value
    // returned is only telling the user what the state of isHalted was at
    // the time is accessed it, and not necessarily the value it has when
    // the user looks at the return value; unless they have the mutex lock
    // before calling this, and keep it until they are done accessing the
    // return value.
    //
    CHECK(pthread_mutex_lock(&g->mutex));

    bool ret = g->isHalted;

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}
