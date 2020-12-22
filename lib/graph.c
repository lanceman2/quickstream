#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "graph.h"
#include "trigger.h"
#include "threadPool.h"
#include "triggerJobsLists.h"
#include "builder.h"
#include "run.h"

// A singly linked list of all graphs.  We do not expect a lot of them.
struct QsGraph *graphs = 0;

pthread_t mainThread;

static bool mainThreadSet = false;


pthread_key_t _qsGraphKey;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

/* Allocate the key */
// We never destroy this key because we can't anticipate when the user
// will make more graphs.  It would appear that this does not make a
// memory leak that Valgrind tests detect.
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
    ASSERT(graph->flowState == QsGraphFlowing);


    if(graph->threadPools->maxThreads != 0) {
        // All the thread pools share these graph resources.
        CHECK(pthread_mutex_destroy(&graph->mutex));
        CHECK(pthread_cond_destroy(&graph->cond));
#ifdef DEBUG
        memset(&graph->mutex, 0, sizeof(graph->mutex));
        memset(&graph->cond, 0, sizeof(graph->cond));
#endif
        DASSERT(graph->masterWaiting == false);
    }

    // All the threads should be joined now.  Free the threads array in
    // all the thread pools.
    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next) {
        CHECK(pthread_cond_destroy(&tp->cond));
#ifdef DEBUG
        tp->mutex = 0;
        if(tp->maxThreads) {
            DASSERT(tp->threads);
            memset(tp->threads, 0, sizeof(*tp->threads)*tp->maxThreads);
        } else
            ASSERT(tp->threads == 0);
#endif
        if(tp->maxThreads) {
            free(tp->threads);
            tp->threads = 0;
            tp->numThreads = 0;
        }
    }

    // Stop all triggers in all blocks.  If we did not interupt the
    // running of the stream this should be done already, but if not ...
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *)b;

        // Empty the job queue; putting the jobs into triggers.
        while(smB->firstJob)
            PopJobBackToTriggers(smB);

        // Go through all the triggers and "stop" them.
        for(struct QsTrigger *t = ((struct QsSimpleBlock *)b)->waiting;
                t; t = t->next)
            TriggerStop(t);
    }


    int ret = 0;
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if((ret = RunStartOrStop(b, b->stop, "stop", _QS_IN_STOP)))
            break;

    // Free the stream ring buffers and ...
    StreamStop(graph);


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


static
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


    // Allocate stream buffers
    if(StreamsStart(graph)) return -1; // error


    // At this point all simple blocks in this graph should have a thread
    // pool assigned to it.


    // The thread pools need an array of pthread_t so they can
    // pthread_join() later.  Since the maximum number of threads is a
    // configuration thing we need to allocate this array separate from
    // allocating the thread pool.
    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next) {
        if(tp->maxThreads == 0) continue;
        DASSERT(tp->threads == 0);
        tp->threads = calloc(tp->maxThreads, sizeof(*tp->threads));
        ASSERT(tp->threads, "calloc(%" PRIu32 ",%zu) failed",
                tp->maxThreads, sizeof(*tp->threads));
    }



    if(graph->threadPools->maxThreads != 0) {

        CHECK(pthread_mutex_init(&graph->mutex, 0));
        CHECK(pthread_cond_init(&graph->cond, 0));
        DASSERT(graph->masterWaiting == false);
    }


    // TODO: HERE set any parameters that are feed by constant parameters.

    return 0; // success
}


int qsGraphRun(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(graph->flowState == QsGraphPaused);

    int ret;
    if((ret = qsGraphReady(graph)))
        return ret;

    DASSERT(graph->threadPools);
    // If we have a thread pool with 0 workers than it better be the only
    // thread pool.
    DASSERT((graph->threadPools->maxThreads == 0 &&
                graph->threadPools->next == 0) ||
        graph->threadPools->maxThreads != 0);

    // Start all triggers in all blocks.
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;

        for(struct QsTrigger *t = ((struct QsSimpleBlock *)b)->waiting;
                t; t = t->next)
            TriggerStart(t);
    }

    ret = 0;
    for(struct QsBlock *b = graph->firstBlock; b; b = b->next)
        if((ret = RunStartOrStop(b, b->start, "start", _QS_IN_START)))
            break;

    if(ret < 0) {
        // One of the block's start() calls returned less than 0.
        graph->flowState = QsGraphFailed;
        // TODO: Free stuff.
        return ret;
    }


    graph->flowState = QsGraphFlowing;

    ret = run(graph);

    if(graph->threadPools->maxThreads == 0 || ret)
        // The user does not necessarily need to call qsGraphWait() if the
        // main thread runs the graph, so we must do this now.
        qsGraphStop(graph);

    return 0;
}


int qsGraphWait(struct QsGraph *graph) {

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph->threadPools);

    if(graph->flowState == QsGraphPaused) {
        // We are already paused.
        return 0;
    }
    ASSERT(graph->flowState == QsGraphFlowing);


    DASSERT(graph->threadPools);
    // This is not the case of the main thread running the graph without
    // any worker threads.
    DASSERT(graph->threadPools->maxThreads != 0);


    CHECK(pthread_mutex_lock(&graph->mutex));
    DASSERT(graph->masterWaiting == false);

    if(graph->numWorkingThreads) {
        // The last worker thread to finish will signal this thread on the
        // way out, but we don't need to wait if there are no worker
        // threads any more.
        graph->masterWaiting = true;
        CHECK(pthread_cond_wait(&graph->cond, &graph->mutex));
        graph->masterWaiting = false;
        DASSERT(graph->numWorkingThreads == 0);
    }

    CHECK(pthread_mutex_unlock(&graph->mutex));

    // In order to know that all the threads are terminated we must join
    // all of them.  We do know, since we wrote this code, that all the
    // worker threads are at the end of their thread work function, but the
    // only way to know that the worker thread are cleaned up is to call
    // pthread_join().  If you don't call pthread_join() for all worker
    // threads than a user doing a quick restart can fail if the threads
    // still exist in the system.  I've seen it fail because of that, like
    // in test code that runs many threads and restarts very quickly.
    //
    // We could not use pthread_join() as a replacement for the waiting
    // call above because we needed to be able to launch threads from the
    // worker threads, and so we needed to sync more, or protect memory
    // more; and that's what the above code does.
    //
    // Because we got the condition signal above we can be sure that no
    // other thread will access this data here:
    for(struct QsThreadPool *tp=graph->threadPools; tp; tp=tp->next) {
        DASSERT(tp->threads);
        DASSERT(tp->maxThreads);
        DASSERT(tp->numThreads != 0);
        // looping backwards on i
        for(uint32_t i = tp->numThreads - 1; i != -1; --i) {
            if(!tp->threads[i].hasLaunched) break;
            // Join a worker thread:
            CHECK(pthread_join(tp->threads[i].thread, 0));
        }
    }

    graph->numWorkingThreads = 0;
    graph->numIdleThreads = 0;

    // At this point there should be no more worker threads for this
    // graph.

    qsGraphStop(graph);


    return 0; // success
}


struct QsBlock *qsGraphGetBlockByName(const struct QsGraph *graph,
        const char *bname) {

    DASSERT(graph);
    DASSERT(graph->blocks);

    return qsDictionaryFind(graph->blocks, bname);
}
