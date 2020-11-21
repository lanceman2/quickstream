
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "app.h"
#include "builder.h"

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
        const char *funcName) {

    if(!st) return 0;

    uint32_t numIn, numOut;
    if(b->isSuperBlock) {
        numIn = 0;
        numOut = 0;
    } else {
        numIn = ((struct QsSimpleBlock *) b)->numInputs;
        numOut = ((struct QsSimpleBlock *) b)->numOutputs;
    }

    int ret;

    if((ret = st(numIn, numOut)) > 0) {
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
        if((ret = RunStartOrStop(b, b->stop, "stop")))
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


struct QsThreadPool *qsGraphThreadPoolCreate(struct QsGraph *graph,
        uint32_t maxThreads) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph);
    ASSERT(graph->flowState == QsGraphPaused);

    struct QsThreadPool *tp = calloc(1, sizeof(*tp));
    ASSERT(tp, "calloc(1,%zu) failed", sizeof(*tp));

    // Add threadPool to the graphs list of thread pools
    tp->next = graph->threadPools;
    graph->threadPools = tp;

    tp->maxThreads = maxThreads;
    tp->graph = graph;

    // This newly created thread pool will be the new default thread pool
    // for all newly created simple blocks, until we make another.  Also
    // old simple blocks in this graph that do not have a thread pool yet
    // will be set to using this thread pool, for now.  The user can move
    // blocks to any thread pool that they like with
    // qsThreadPoolAddBlock().

    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if(!b->isSuperBlock && !((struct QsSimpleBlock *) b)->threadPool)
            qsThreadPoolAddBlock(tp, b);

    return tp;
}


void qsThreadPoolDestroy(struct QsThreadPool *tp) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(tp);
    struct QsGraph *graph = tp->graph;
    DASSERT(graph);
    ASSERT(graph->flowState == QsGraphPaused ||
            graph->flowState == QsGraphFailed);

    {
        // Remove this thread pool, tp, from the graph thread pool list.
        //
        struct QsThreadPool *prev = 0;
        struct QsThreadPool *t;
        for(t = graph->threadPools; t && t != tp; tp = tp->next)
            prev = t;
        DASSERT(t);
        DASSERT(t == tp);
        if(prev)
            prev->next = tp->next;
        else
            graph->threadPools = tp->next;
    }

#ifdef DEBUG
    memset(tp, 0, sizeof(*tp));
#endif

    free(tp);
}


void qsThreadPoolAddBlock(struct QsThreadPool *tp, struct QsBlock *b) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(tp);
    DASSERT(tp->graph);
    DASSERT(b);
    ASSERT(b->isSuperBlock == 0,
            "Super Blocks cannot gets added to thread pools");
    ASSERT(b->graph == tp->graph,
            "The block does not belong to the "
            "same graph as the thread pool");
    ASSERT(tp->graph->flowState == QsGraphPaused);

    ((struct QsSimpleBlock *) b)->threadPool = tp;
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


    // TODO: Allocate stream buffers


    int ret = 0;
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if((ret = RunStartOrStop(b, b->start, "start")))
            break;

    if(ret < 0) {
        // One of the block's start() calls returned less than 0.
        graph->flowState = QsGraphFailed;
        // TODO: Free stuff.
        return ret;
    }

ERROR();

    graph->flowState = QsGraphReady;

    return 0; // success
}


int qsGraphRun(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(graph->flowState != QsGraphFlowing &&
            graph->flowState != QsGraphFailed);

    int ret;

    if(graph->flowState == QsGraphPaused)
        if((ret = qsGraphReady(graph)))
            return ret;




    DASSERT(graph->flowState == QsGraphReady);

    graph->flowState = QsGraphFlowing;

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
