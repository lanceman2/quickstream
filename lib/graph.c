
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "graph.h"
#include "threadPool.h"
#include "builder.h"
#include "run.h"

// A singly linked list of all graphs.  We do not expect a lot of them.
struct QsGraph *graphs = 0;

pthread_t mainThread;

static bool mainThreadSet = false;


pthread_key_t _qsGraphKey;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

/* Allocate the key */
static void make_key() {
    CHECK(pthread_key_create(&_qsGraphKey, 0));
}


struct QsGraph *qsGraphCreate(void) {

    CHECK(pthread_once(&key_once, make_key));

    struct QsGraph *graph = calloc(1, sizeof(*graph));
    ASSERT(graph, "calloc(1,%zu) failed", sizeof(*graph));
    graph->blocks = qsDictionaryCreate();
    DASSERT(graph->blocks);

    if(mainThreadSet == false) {
        mainThread = pthread_self();
        mainThreadSet = true;
    } else
        ASSERT(mainThread == pthread_self(), "Not graph main thread");

    // Add this to the list of graphs
    graph->next = graphs;
    graphs = graph;

    return graph;
}


static inline
int RunStartOrStop(struct QsBlock *b,
        int (*st)(uint32_t numIn, uint32_t numOut),
        const char *funcName, uint32_t cbType) {

    if(!st) return 0;

    uint32_t numIn, numOut;
    if(b->isSuperBlock) {
        numIn = 0;
        numOut = 0;
    } else {
        numIn = ((struct QsSimpleBlock *) b)->numInputs;
        numOut = ((struct QsSimpleBlock *) b)->numOutputs;
    }

    DASSERT(b->inWhichCallback == _QS_IN_NONE);
    b->inWhichCallback = cbType;
    // Make it so the start() or stop() call can tell what block it is
    // using pthread_getspecific().
    DASSERT(pthread_getspecific(_qsGraphKey) == 0);
    CHECK(pthread_setspecific(_qsGraphKey, b));
    int ret = st(numIn, numOut);
    CHECK(pthread_setspecific(_qsGraphKey, 0));
    b->inWhichCallback = _QS_IN_NONE;

    if(ret > 0) {
        NOTICE("Removing block's \"%s\" %s() callback",
                b->name, funcName);
        b->start = 0;
        return 0;
    } else if(ret < 0) {
        ERROR("Block \"%s\" %s()=%d callback failed",
                b->name, funcName, ret);
        return ret;
    }
    return 0;
}


static
int qsGraphStop(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(graph->flowState == QsGraphFlowing ||
            graph->flowState == QsGraphReady);


    int ret = 0;
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if((ret = RunStartOrStop(b, b->stop, "stop", _QS_IN_STOP)))
            break;


    if(ret < 0) {
        // One of the block's stop() calls returned less than 0.
        graph->flowState = QsGraphFailed;
        // TODO: Free stuff.
        return ret;
    }

    graph->flowState = QsGraphPaused;

    return 0; // success
}


void qsGraphDestroy(struct QsGraph *graph) {
 
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph);
    DASSERT(graph->blocks);
    ASSERT(graph->flowState != QsGraphFlowing);

    if(graph->flowState == QsGraphReady)
        qsGraphStop(graph);

    {
        // Destroy the thread pools:
        struct  QsThreadPool *next;
        for(struct QsThreadPool *tp = graph->threadPools; tp; tp = next) {
            next = tp->next;
            qsThreadPoolDestroy(tp);
        }
    }

    // Remove this from the list of graphs
    DASSERT(graphs);
    if(graphs == graph)
        graphs = graph->next;
    else {
        struct QsGraph *prev = graphs;
        while(prev && prev->next != graph) prev = prev->next;
        DASSERT(prev);
        DASSERT(prev->next == graph);
        prev->next = graph->next;
    }

    // This will destroy all the blocks too, via the
    // qsDictionarySetFreeValueOnDestroy() thingy.
    qsDictionaryDestroy(graph->blocks);

#ifdef DEBUG
    memset(graph, 0, sizeof(*graph));
#endif

    free(graph);
}


int qsGraphReady(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(graph->flowState == QsGraphPaused);

    // Check for thread pools structures.  Not making threads yet, just
    // data.
    if(graph->threadPools == 0)
        // Make a default thread pool.
        qsGraphThreadPoolCreate(graph, QS_DEFAULT_MAXTHREADS);
    else {
        bool haveZeroMaxThreads = false;
        // Check for an invalid graph thread pools case.
        for(struct QsThreadPool *tp = graph->threadPools; tp;
                tp = tp->next)
            if(tp->numThreads == 0) {
                haveZeroMaxThreads = true;
                break;
            }

        if(haveZeroMaxThreads && graph->threadPools->next) {
            // We have a main thread running a thread pool and at the same
            // time we have yet another thread pool.  That hurts my head
            // too much to think about.  So fuck it.
            ERROR("Mick says: \"You can't always get what you want.\""
                    "You have a thread pool with 0 threads and more than"
                    " one thread pool total.");
            // We may yet recover from this, if they reconfigure the
            // thread pools.
            return -1; // error
        }
    }

    // It could easily happen that there is a thread pool with no assigned
    // blocks for it to run.  Thread pools like that are of no use, and
    // just waist memory and add extra code to let them exist.  Design
    // decision: We choice to make them disappear here with just a
    // warning.
    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next) {
        struct QsBlock *b;
        for(b = graph->firstBlock; b; b = b->next) {
            DASSERT(b->isSuperBlock ||
                    ((struct QsSimpleBlock *)b)->threadPool);
            if(!b->isSuperBlock &&
                    ((struct QsSimpleBlock *)b)->threadPool == tp)
                break;
        }
        if(!b) {
            // We have not found a block that is using thread pool, tp,
            // so we get rid of it.
            WARN("Removing empty thread pool");
            qsThreadPoolDestroy(tp);
        }
    }

    if(!graph->threadPools) {
        // From above, if there are no thread pools than there are no
        // simple blocks.  Super blocks do not have triggers.  If there
        // are no simple blocks, there is nothing to run.
        ERROR("No simple blocks in graph");
        return -1; // error
    }


    // At this point all simple blocks in this graph should have a thread
    // pool assigned to it.


    // TODO: HERE Allocate stream buffers


    // TODO: HERE set any parameters that are feed by constant parameters.


    int ret = 0;
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if((ret = RunStartOrStop(b, b->start, "start", _QS_IN_START)))
            break;

    if(ret < 0) {
        // One of the block's start() calls returned less than 0.
        graph->flowState = QsGraphFailed;
        // TODO: Free stuff.
        return ret;
    }

    graph->flowState = QsGraphReady;

    return 0; // success
}


int qsGraphRun(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(graph->flowState != QsGraphFlowing &&
            graph->flowState != QsGraphFailed);

    if(graph->flowState == QsGraphPaused) {
        int ret;
        if((ret = qsGraphReady(graph)))
            return ret;
    }

    DASSERT(graph->flowState == QsGraphReady);
    DASSERT(graph->threadPools);
    // If we have a thread pool with 0 workers than it better be the only
    // thread pool.
    DASSERT((graph->threadPools->maxThreads == 0 &&
                graph->threadPools->next == 0) ||
        graph->threadPools->maxThreads != 0);

    graph->flowState = QsGraphFlowing;

    run(graph);

    return 0;
}


int qsGraphWait(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(graph->flowState == QsGraphFlowing);
    DASSERT(graph->threadPools);


    // MORE HERE


    qsGraphStop(graph);

    // success.
    graph->flowState = QsGraphPaused;


    return 0; // success
}

struct QsBlock *qsGraphGetBlockByName(struct QsGraph *graph,
        const char *bname) {

    DASSERT(graph);
    DASSERT(graph->blocks);

    return (struct QsBlock *) qsDictionaryFind(graph->blocks, bname);
}
