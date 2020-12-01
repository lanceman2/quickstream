
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


// Return the first block in the line and remove it from the list.
//
// static inline
struct QsSimpleBlock *PopThreadPoolQueue(struct QsThreadPool *tp) {

    DASSERT(tp);

    struct QsSimpleBlock *b = tp->first;

    DASSERT(!b || b->prev == 0);

    if(b) {
        DASSERT(tp->last);
        tp->first = b->next;
        if(b->next) {
            DASSERT(b->next->prev == b);
            b->next->prev = 0;
            b->next = 0;
        } else {
            DASSERT(b == tp->last);
            tp->last = 0;
        }
    }

    return b;
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

    // b should not be in a thread pool queue yet:
    DASSERT(((struct QsSimpleBlock *) b)->next == 0);
    DASSERT(((struct QsSimpleBlock *) b)->prev == 0);

    // There should be nothing in the thread pool queue yet:
    DASSERT(tp->first == 0);
    DASSERT(tp->last == 0);
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
