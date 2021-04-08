
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


// The number of thread pools created.  Nothing to do with the number
// destroyed.  Only increases.  We let the first thread pool ID be 0 (not
// 1).
//
static uint32_t createCount = 0;


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

    qsDictionaryRemove(graph->threadPoolDict, tp->name);

#ifdef DEBUG
    memset(tp->name, 0, strlen(tp->name));
#endif
    free(tp->name);


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
    ((struct QsSimpleBlock *) b)->threadPool = tp;
}


struct QsThreadPool *qsGraphThreadPoolCreate(struct QsGraph *graph,
        uint32_t maxThreads, const char *threadPoolName) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph);
    ASSERT(graph->flowState == QsGraphPaused);

    // Get unique thread pool name, for this graph.
#define NAME_LEN   (128)
    ASSERT(!threadPoolName || strlen(threadPoolName) < NAME_LEN);

    char *name;

    if(threadPoolName) {
         if(qsDictionaryFind(graph->threadPoolDict, threadPoolName)) {
                //
                // Because they requested a particular name and the name
                // is already taken, we can fail here.
                //
                ERROR("ThreadPool name \"%s\" is in use already",
                        threadPoolName);
                return 0;
         }
        name = strdup(threadPoolName);
    } else {
        char nam[NAME_LEN];
        uint32_t count = createCount;
        do {
            if(count > createCount + 1000) {
                // Can't imagine this will happen.
                ERROR("Can't get thread pool name");
                return 0;
            }
            snprintf(nam, NAME_LEN, "Thread Pool %" PRIu32, ++count);
        } while(qsDictionaryFind(graph->threadPoolDict, nam));
        name = strdup(nam);
    }

    ASSERT(name, "strdup() failed");


    struct QsThreadPool *tp = calloc(1, sizeof(*tp));
    ASSERT(tp, "calloc(1,%zu) failed", sizeof(*tp));

    // Add threadPool to the graphs list of thread pools
    tp->next = graph->threadPools;
    graph->threadPools = tp;

    tp->maxThreads = maxThreads;
    tp->graph = graph;
    tp->id = createCount++;
    tp->name = name;

    // This newly created thread pool will be the new default thread pool
    // for all newly created simple blocks, until we make another.  Also
    // old simple blocks in this graph that do not have a thread pool yet
    // will be set to using this thread pool, for now.  The user can move
    // blocks to any thread pool that they like with
    // qsThreadPoolAddBlock().
    //
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if(!b->isSuperBlock && !((struct QsSimpleBlock *) b)->threadPool)
            qsThreadPoolAddBlock(tp, b);

    // Add thread pool to the graph dictionary thread pool list.
    ASSERT(0 == qsDictionaryInsert(graph->threadPoolDict,
                tp->name, tp, 0));

    return tp;
}


struct QsThreadPool *qsGraphThreadPoolGetByName(struct QsGraph *graph,
        const char *threadPoolName) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph);
    ASSERT(graph->flowState == QsGraphPaused);
    DASSERT(threadPoolName);

    return qsDictionaryFind(graph->threadPoolDict, threadPoolName);
}


int qsThreadPoolSetName(struct QsThreadPool *tp,
        const char *threadPoolName) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(tp);
    struct QsGraph *g = tp->graph;
    DASSERT(g);
    ASSERT(g->flowState == QsGraphPaused);

    DASSERT(tp->name);
    DASSERT(qsDictionaryFind(g->threadPoolDict, tp->name));

    if(qsDictionaryFind(g->threadPoolDict, threadPoolName)) {
        ERROR("ThreadPool name \"%s\" is in use already",
                threadPoolName);
        return 1;
    }

    // Remove the thread pool from the graph dictionary thread pool list.
    qsDictionaryRemove(g->threadPoolDict, tp->name);

#ifdef DEBUG
    memset(tp->name, 0, strlen(tp->name));
#endif
    free(tp->name);

    tp->name = strdup(threadPoolName);
    ASSERT(tp->name, "strdup() failed");

    // Add thread pool back to the graph dictionary thread pool list.
    ASSERT(0 == qsDictionaryInsert(g->threadPoolDict, tp->name, tp, 0));

    return 0; // success.
}


void qsThreadPoolSetMaxThreads(struct QsThreadPool *tp,
        uint32_t maxThreads) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(tp);
    struct QsGraph *g = tp->graph;
    DASSERT(g);
    ASSERT(g->flowState == QsGraphPaused);

    tp->maxThreads = maxThreads;
}
