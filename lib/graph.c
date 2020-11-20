
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


void qsGraphDestroy(struct QsGraph *graph) {
 
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph);
    DASSERT(graph->blocks);

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
    DASSERT(tp->graph);

    struct QsGraph *graph = tp->graph;

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

    ((struct QsSimpleBlock *) b)->threadPool = tp;
}


int qsGraphReady(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    ASSERT(graph->flowState == QsGraphPaused);

    // TODO: Allocate stream buffers

    int ret = 0;
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next) {
        if(b->start) {
            uint32_t numIn, numOut;
            if(b->isSuperBlock) {
                numIn = 0;
                numOut = 0;
            } else {
                numIn = ((struct QsSimpleBlock *) b)->numInputs;
                numOut = ((struct QsSimpleBlock *) b)->numOutputs;
            }

            if((ret = b->start(numIn, numOut)) > 0) {
                NOTICE("Removing block \"%s\" start() callback",
                        b->name);
                b->start = 0;
            } else if(ret < 0) {
                ERROR("Block \"%s\" start() callback failed",
                        b->name);
                break;
            }
        }
    }

    if(ret < 0) {
        // One of the block's start() calls returned less than 0.
        graph->flowState = QsGraphFailed;
        // TODO: Free stuff.
        return ret;
    }

    graph->flowState = QsGraphReady;

    return 0; // success
}
