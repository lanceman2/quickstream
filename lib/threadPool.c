#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
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
#include "port.h"
#include "job.h"
#include "stream.h"



// We must have a thread pool mutex lock before calling this.  This
// returns while holding the thread pool mutex lock.
//
// Try to keep this function simple, and put the crap (complexity) in the
// PopBlock(), PopJob(), qsJob_work(j), and the other block/job queuing
// functions.
//
// The struct QsWhichJob *wj is the thread specific data for this worker
// thread.  We change wj->job before we work on it.
//
static inline
bool WorkerLoop(struct QsThreadPool *tp, struct QsWhichJob *wj) {

    DASSERT(tp);
    DASSERT(tp->numWorkingThreads <= tp->numThreads);


    if(tp->first && tp->first->next)
        // We have 2 blocks ready for a worker thread.
        CheckLaunchWorkers(tp);

    struct QsJobsBlock *b;

    while((b = PopBlock(tp))) {

        DASSERT(!b->busy);
        b->busy = true;
        // So now this block will not get queued in this thread pool's
        // block queue; it's "busy".

        struct QsJob *j = PopJob(b);
        // If the block popped it must have at least one job.
        DASSERT(j);

        do {

            DASSERT(!j->inQueue);
            DASSERT(!j->busy);
            j->busy = true;

            CHECK(pthread_mutex_unlock(&tp->mutex));
  
            // Process job/event.
            qsJob_work(j, wj);

            CHECK(pthread_mutex_lock(&tp->mutex));

            DASSERT(!j->inQueue);
            DASSERT(j->busy);

            j->busy = false;


            if(j->work == (void *) StreamWork &&
                    --tp->graph->streamJobCount == 0)
                // streamJobCount is an atomic counter that
                // is counting the number of stream jobs queued.
                CheckStreamFinished(tp->graph);


            qsJob_unlock(j);


        } while((j = PopJob(b)));

        DASSERT(b->busy);
        b->busy = false;
        // So now this block will can get queued in this thread pool's
        // block queue; it's not "busy" any more.
    }

    if(tp->numThreads > tp->maxThreadsRun)
        // This worker is being terminated, before it would have waited.
        return false;


    --tp->numWorkingThreads;

    if(tp->haltCond && tp->numWorkingThreads == 0)
        // Last worker to sleep (wait) signal this before we sleep (wait).
        CHECK(pthread_cond_signal(tp->haltCond));

    do
        // It will return some time after we signal it, but we don't know
        // when.  The signalOne flag lets us know that we are waiting for
        // a thread to return from this pthread_cond_wait() because it was
        // signaled, without having to have the signaler wait for a reply.
        //
        // We only let them out one at a time.  If they are too slow to
        // trigger (return from pthread_cond_wait()) then the signal may
        // not let a worker thread out of this do while loop.  Like if we
        // signal it, than some time later we "close the gate" by setting
        // tp->signalingOne to false in another part of the code.
        //
        CHECK(pthread_cond_wait(&tp->cond, &tp->mutex));
    while(!tp->signalingOne);

    tp->signalingOne = false;

    ++tp->numWorkingThreads;

    if(tp->numThreads > tp->maxThreadsRun)
        // This worker is being terminated, after it waited.
        return false;


    return true; // keep looping
}


static void *RunThread(struct QsThreadPool *tp) {

    DASSERT(tp);

    struct QsWhichJob *wj = calloc(1, sizeof(*wj));
    ASSERT(wj, "calloc(1,%zu) failed", sizeof(*wj));
    wj->threadPool = tp;

    CHECK(pthread_setspecific(threadPoolKey, wj));

    // This worker thread is already counted by tp->numThreads and
    // tp->numWorkingThreads.  We are pretending that it is working now;
    // which is fine so long as we keep this code consistent.

    // Imagine that this thread hangs here, at this line in the code, in
    // the kernels task scheduler for a long time.  In that time another
    // thread could remove all the blocks and try to end this process.  In
    // any case we can't be sure what the thread pool will be doing
    // until we get this thread pool mutex lock.
    //
    CHECK(pthread_mutex_lock(&tp->mutex));

    // Now we know we really have a new worker thread.  At least that's
    // what we think of getting the thread pool mutex lock does for us.

    DASSERT(tp->numWorkingThreads <= tp->numThreads);

    while(WorkerLoop(tp, wj));

    // We make it so that only one thread pool worker thread exits at a
    // time, via the thread pool mutex lock and the maxThreadsRun and
    // numThreads counters.

    DASSERT(tp->numThreads = tp->maxThreadsRun + 1);

    --tp->numWorkingThreads;
    --tp->numThreads;

    DSPEW("Thread pool \"%s\" has %" PRIu32 " worker threads",
            tp->name, tp->numThreads);
 

    // Tell the thread that is joining this thread who we are:
    tp->exitingPthread = pthread_self();

    CHECK(pthread_cond_signal(&tp->wakerCond));
    CHECK(pthread_mutex_unlock(&tp->mutex));

#ifdef DEBUG
    CHECK(pthread_setspecific(threadPoolKey, 0));
    memset(wj, 0, sizeof(*wj));
#endif
    free(wj);

    // After this thread joins another thread can get the thread pool
    // mutex lock and decrease maxThreadsRun by one and so on ...
    //
    // See JoinThreads().

    return 0;
}


// We need a thread pool mutex lock to call this:
//
void _LaunchWorker(struct QsThreadPool *tp) {

    DASSERT(tp->numWorkingThreads <= tp->numThreads);
    DASSERT(tp->numThreads < tp->maxThreadsRun);

#if 0
    if(tp->numThreads >= tp->maxThreadsRun)
        // This happened to slowly.
        return;
#endif

    DASSERT(tp->numThreads < tp->numJobsBlocks ||
            (tp->numJobsBlocks == 0 && tp->numThreads == 0));
    DASSERT(tp->numThreads < tp->maxThreads);

    pthread_t p;

    CHECK(pthread_create(&p, 0, (void *(*) (void *)) RunThread, tp));

    // Count this new worker thread NOW.  We want these numbers to reflect
    // that there is another worker thread now, so that we do not make
    // more after we release the thread pool mutex below in
    // pthread_cond_wait().
    //
    // It's a BUG to not set these counters here, before the RunThread()
    // function gets the thread pool mutex, because another worker thread
    // could intercede between here and the running of RunThread() and
    // launch more worker threads before the new worker thread get the
    // thread pool mutex, if we do not count them now.
    //
    // The test "tests/104_varyMaxThreads" caught this BUG.  It was
    // cleaning up the thread pool pthread mutexes and condition
    // variables, assuming that there was no worker threads running, when
    // there were worker threads running (just starting), but the
    // counters where not reflecting that.  The bug was very hard to find
    // given that the program would only crash long after the
    // "miss-count".
    //
    ++tp->numThreads;
    ++tp->numWorkingThreads;

    DSPEW("Thread pool \"%s\" has %" PRIu32 " worker threads",
            tp->name, tp->numThreads);
}


void LaunchWorker(struct QsThreadPool *tp) {

    DASSERT(tp);
    DASSERT(tp->name);
    DASSERT(tp->graph);

    CHECK(pthread_mutex_lock(&tp->mutex));

    _LaunchWorker(tp);

    CHECK(pthread_mutex_unlock(&tp->mutex));
}


static inline
void _qsThreadPool_addBlock(struct QsThreadPool *tp, struct QsBlock *b,
        bool fixPeerThreadPoolList) {

    struct QsJobsBlock *jb = (struct QsJobsBlock *) b;
    struct QsThreadPool *oldTp = jb->threadPool;
    DASSERT(oldTp);
    // These thread pools should be halted.
    DASSERT(oldTp->halt);
    DASSERT(tp->halt);
    DASSERT(!oldTp->numWorkingThreads);
    DASSERT(!tp->numWorkingThreads,
            "thread pool \"%s\" is not halted", tp->name);

    if(oldTp == tp) return;

    bool gotJobs = PullBlock(oldTp, jb);


    Block_removeThreadPool(jb, oldTp);
    Block_addThreadPool(jb, tp);


    // Move all the peer jobs block thread pool lists to the new
    // thread pool.  For the case when a thread pool is destroyed
    // this has to be done before now.
    if(fixPeerThreadPoolList)
        for(struct QsJob *j = jb->jobsStack; j; j = j->n) {

            if(j->sharedPeers) {
                // For the case where we have all the jobs in a job group
                // shared the group list, therefore we keep the jobs block
                // thread pools lists differently.  Hence we just move
                // from the "oldTp" thread pool to the "tp" thread pool
                // just once.
                DASSERT(j->jobsBlock);
                DASSERT(j->jobsBlock->threadPool);
                if(j->jobsBlock->threadPool != oldTp)
                    continue;
                Block_removeThreadPool(jb, oldTp);
                Block_addThreadPool(jb, tp);
                continue;
            }
            for(struct QsJob **pj = j->peers; *pj; ++pj) {
                DASSERT((*pj)->jobsBlock);
                if((*pj)->jobsBlock == jb)
                    continue;
                Block_removeThreadPool((*pj)->jobsBlock, oldTp);
                Block_addThreadPool((*pj)->jobsBlock, tp);
            }
        }


    jb->threadPool = tp;
    --oldTp->numJobsBlocks;
    ++tp->numJobsBlocks;

    tp->maxThreadsRun = Get_maxThreadsRun(tp);
    uint32_t maxThreadsRun = Get_maxThreadsRun(oldTp);

    if(oldTp->numThreads > maxThreadsRun) {
        // Kill some worker threads that are not needed because this block
        // was removed from that thread pool.
        CHECK(pthread_mutex_lock(&oldTp->mutex));
        JoinThreads(oldTp, maxThreadsRun);
        CHECK(pthread_mutex_unlock(&oldTp->mutex));
    } else
        oldTp->maxThreadsRun = maxThreadsRun;


    // If new worker threads can run in thread pool, tp, then they will
    // automatically launch from the main worker loop.

    if(gotJobs)
        QueueBlock(tp, jb);
}


void qsThreadPool_addBlock(struct QsThreadPool *tp, struct QsBlock *b) {

    NotWorkerThread();
    DASSERT(b);
    // This must be a block of type with QsJobsBlock in it.
    ASSERT(b->type & QS_TYPE_JOBS);
    DASSERT(tp);
    struct QsGraph *g = tp->graph;
    DASSERT(b->graph);
    DASSERT(b->graph == g);

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    if(((struct QsJobsBlock *)b)->threadPool == tp) {
        CHECK(pthread_mutex_unlock(&g->mutex));
        return;
    }

    // I do not think we have a list of thread pools that is needed to
    // halt in this case, so we'll halt all of them that are in the
    // graph.
    //
    // Think about it.  We are moving a block to what could be a thread
    // pool that is (job) connected to lots of other thread pools that we
    // do not have in any list at this time.  In order to compose a thread
    // pool list we need to halt all the thread pools or what ...
    //
    // The point is: halting fewer thread pools is better, but only if we
    // are guaranteed not to corrupt the block and job queues.
    //
    // That's what it takes to move blocks between thread pools, on the
    // fly.
    qsGraph_threadPoolHaltLock(g, 0);

    _qsThreadPool_addBlock(tp, b, true);

    qsGraph_threadPoolHaltUnlock(g);

    CHECK(pthread_mutex_unlock(&g->mutex));
}

// numThreadsRun is the target number of threads that we want as
// tp->numThreads when this returns.
//
// We need a thread pool mutex lock before calling this.
//
void JoinThreads(struct QsThreadPool *tp, uint32_t numThreadsRun) {

    DASSERT(tp->maxThreadsRun >= tp->numThreads);

    // Signal worker threads and join them one at a time;
    // until we get tp->numThreads == numThreadsRun
    // and tp->numThreads == tp->maxThreadsRun
    //
    // This is nice!  You see, the threads cannot all exit at the same
    // time anyway, because they all need to access the same QsThreadPool
    // data as they exit and they can't access the same QsThreadPool data
    // at the same time.  This is as simple as it gets.
    //
    // This should be much faster than letting signaling all the threads
    // to exist at the same time, due to there being much less
    // inter-thread contention, in the mutex locking.  I like to think
    // that's the case.  And I don't need to keep a list (array) of all
    // the pthread_t variables; just tp->exitingPthread, one at a time.


    while(tp->numThreads > numThreadsRun) {

        // Signal workers and join them one at a time.

        // tp->maxThreadsRun is the target number of threads.
        //
        tp->maxThreadsRun = tp->numThreads - 1; // one at a time...

        if(!tp->numWorkingThreads) {
            tp->signalingOne = true;
            CHECK(pthread_cond_signal(&tp->cond));
        }

        CHECK(pthread_cond_wait(&tp->wakerCond, &tp->mutex));

        CHECK(pthread_join(tp->exitingPthread, 0));

        DSPEW("Thread pool \"%s\" joined worker thread[%" PRIu32 "] %"
                PRIu32 " thread(s) remaining",
                tp->name, tp->numThreads, tp->numThreads);
    }

    // It can be the case that all the remaining thread pool worker
    // threads that are all left are in pthread_cond_wait(&tp->cond,
    // &tp->mutex) even though there are jobs in the thread pool queue.
    // If that is the case we have to wake one up.  The woken thread will
    // woke more if it sees the need too.
    //
    // Ya, this is a BUG fix.  Pretty sweet solution, I'd say.  A very
    // local small block of code, here:
    //
    if(tp->numThreads && !tp->numWorkingThreads/*they are all waiting*/
            && !tp->halt/*not halted*/ && tp->first/*there is a job*/ &&
            !tp->signalingOne/*one worker at a time please*/)
        if(!tp->numWorkingThreads) {
            // This is a transit thing, so we can spew.
            DSPEW("Waking a sleepy worker after firing "
                    "a different worker");
            tp->signalingOne = true;
            CHECK(pthread_cond_signal(&tp->cond));
        }
}


// The All the graph's thread pools must be halted when this is called.
//
// This function is recursive.  It accesses all blocks in the graph.
//
static inline
void SetJobsBlocksThreadPools(struct QsBlock *b,
        struct QsThreadPool *from, struct QsThreadPool *to) {

    if(b->type & QS_TYPE_JOBS && from ==
            ((struct QsJobsBlock *)b)->threadPool)
        // Move this block, b, to another thread pool, to.
        _qsThreadPool_addBlock(to, b, false);

    if(b->type & QS_TYPE_PARENT)
        // Now act on all the children.
        for(struct QsBlock *child =
                ((struct QsParentBlock *) b)->firstChild;
                child; child = child->nextSibling)
            SetJobsBlocksThreadPools(child, from, to);
}


void qsThreadPool_setMaxThreads(struct QsThreadPool *tp,
        uint32_t maxThreads) {

    NotWorkerThread();
    ASSERT(maxThreads);
    DASSERT(tp);
    DASSERT(tp->name);
    struct QsGraph *g = tp->graph;
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    CHECK(pthread_mutex_lock(&tp->mutex));

    tp->maxThreads = maxThreads;

    uint32_t maxThreadsRun = Get_maxThreadsRun(tp);

    if(tp->numThreads > maxThreadsRun)
        JoinThreads(tp, maxThreadsRun);
    else
        // In the case there we will gain worker threads, that will happen
        // in the WorkerLoop() in PopBlock() as we see that there are no
        // idle worker threads, that is tp->numWorkingThreads == 0.
        tp->maxThreadsRun = maxThreadsRun;


    CHECK(pthread_mutex_unlock(&tp->mutex));

    CHECK(pthread_mutex_unlock(&g->mutex));
}


// The All the graph's thread pools must be halted when this is called.
//
// This function is recursive.  It accesses all blocks in the graph.
//
// Called when the "from" thread pool is being destroyed.
//
static inline
void SetJobsBlocksThreadPoolsLists(struct QsBlock *b,
        struct QsThreadPool *from, struct QsThreadPool *to) {

    if(b->type & QS_TYPE_JOBS) {
        struct QsJobsBlock *jb = (struct QsJobsBlock *) b;
        // Move all the peer jobs block thread pool lists that are
        // "from" to the new thread pool, "to".
        //
        for(struct QsJob *j = jb->jobsStack; j; j = j->n) {

            if(j->sharedPeers) {
                for(struct QsJob **pj = *j->sharedPeers; *pj; ++pj) {
                    DASSERT((*pj)->jobsBlock);
                    DASSERT((*pj)->jobsBlock->threadPool);
                    if((*pj)->jobsBlock->threadPool != from ||
                            // For the shared peer jobs list the list
                            // includes this job, jb, and we did not add
                            // it to it's thread pool list, so we do not
                            // need to move this non-existent entry now.
                            (*pj)->jobsBlock == jb)
                        continue;
                    Block_removeThreadPool((*pj)->jobsBlock, from);
                    Block_addThreadPool((*pj)->jobsBlock, to);
                }
                continue;
            }

            for(struct QsJob **pj = j->peers; *pj; ++pj) {
                DASSERT((*pj)->jobsBlock);
                // For the non-shared peer jobs list does not include this
                // job, jb.
                DASSERT((*pj)->jobsBlock != jb);
                DASSERT((*pj)->jobsBlock->threadPool);
                if((*pj)->jobsBlock->threadPool != from)
                    continue;
                // Change the thread pool list in block jb for a
                // peer job from a different
                // block, ((*pj)->jobsBlock, from "from" to "to":
                Block_removeThreadPool(jb, from);
                Block_addThreadPool(jb, to);
            }
        }

        DASSERT(!(b->type & QS_TYPE_PARENT));

    } else if(b->type & QS_TYPE_PARENT)
        // Now act on all the children.
        for(struct QsBlock *child =
                ((struct QsParentBlock *) b)->firstChild;
                child; child = child->nextSibling)
            SetJobsBlocksThreadPoolsLists(child, from, to);
}


void _qsThreadPool_destroy(struct QsThreadPool *tp) {

    NotWorkerThread();
    DASSERT(tp);
    struct QsGraph *g = tp->graph;
    DASSERT(g);

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    DASSERT(tp->name);

    // TODO: Halt fewer thread pools ???
    // I don't know if we can halt fewer thread pools.
    //
    qsGraph_threadPoolHaltLock(g, 0);


    DSPEW("Destroying thread pool \"%s\"", tp->name);

    DASSERT(tp->name);


    if(g->numThreadPools > 1) {

        struct QsThreadPool *defaultTp = g->threadPoolStack;
        DASSERT(defaultTp);
        DASSERT(defaultTp->next);

        if(defaultTp == tp)
            defaultTp = defaultTp->next;

        DASSERT(defaultTp != tp);
        // Transfer all blocks assigned to this thread pool, tp, to the
        // next default thread pool from g->threadPoolStack:
        //
        SetJobsBlocksThreadPoolsLists(&g->parentBlock.block, tp, defaultTp);
        SetJobsBlocksThreadPools(&g->parentBlock.block, tp, defaultTp);
    }
#ifdef DEBUG
    else {
        // We must be calling qsGraph_destroy() now.
        //
        // There should be no more blocks except the graph:
        ASSERT(!g->parentBlock.firstChild);
        ASSERT(!g->parentBlock.lastChild);
        ASSERT(g->numThreadPools);
    }
#endif


    DASSERT(tp->numJobsBlocks == 0);

    CHECK(pthread_mutex_lock(&tp->mutex));
    JoinThreads(tp, 0); // bring it down to 0 threads.
    CHECK(pthread_mutex_unlock(&tp->mutex));

    DASSERT(tp->numThreads == 0);
    DASSERT(tp->numWorkingThreads == 0);

    qsGraph_threadPoolHaltUnlock(g);


    // In the code above the thread pool with not destroyed yet.  It just
    // has no worker threads now.  I.e. the struct QsThreadPool data was
    // still in working order.

    // Remove this thread pool, tp, from the graphs lists. Since this
    // function is the FreeValueOnDestroy Dictionary function that is
    // automatic.
    --g->numThreadPools;

    // Remove this thread pool, tp, from the graph's threadPoolStack.
    if(tp->prev) {
        DASSERT(tp != g->threadPoolStack);
        tp->prev->next = tp->next;
    } else {
        DASSERT(tp == g->threadPoolStack);
#ifdef SPEW_LEVEL_DEBUG
        // There is always at least one thread pool in the graph
        // unless we are destroying the graph.
        if(!tp->next)
            DSPEW("Destroying last thread pool for graph \"%s\"",
                    g->name);
#endif
        g->threadPoolStack = tp->next;
    }
    //
    if(tp->next)
        tp->next->prev = tp->prev;

    // Remove the thread pool dictionary entry:
    ASSERT(qsDictionaryRemove(g->threadPools, tp->name) == 0);

    if(!g->numThreadPools) {
        DASSERT(!g->threadPoolStack);
        qsDictionaryDestroy(g->threadPools);
        g->threadPools = 0;
    }

    CHECK(pthread_cond_destroy(&tp->cond));
    CHECK(pthread_cond_destroy(&tp->wakerCond));
    CHECK(pthread_mutex_destroy(&tp->mutex));


    DZMEM(tp->name, strlen(tp->name));
    free(tp->name);

    DZMEM(tp, sizeof(*tp));
    free(tp);

    CHECK(pthread_mutex_unlock(&g->mutex));
}


void qsThreadPool_destroy(struct QsThreadPool *tp) {

    NotWorkerThread();
    DASSERT(tp);
    struct QsGraph *g = tp->graph;
    DASSERT(g);

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    ASSERT(g->numThreadPools >= 1,
            "You cannot destroy the last thread pool before "
            "destroying the graph");

    _qsThreadPool_destroy(tp);

    CHECK(pthread_mutex_unlock(&g->mutex));
}


struct QsThreadPool *_qsGraph_createThreadPool(struct QsGraph *g,
        uint32_t maxThreads, const char *name) {

    DASSERT(g);
    ASSERT(maxThreads > 0);
    struct QsThreadPool *tp = 0;

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    name = GetUniqueName(g->threadPools, g->numThreadPools, name, "tp");

    if(!name)
        // Failure.
        goto finish;


    tp = calloc(1, sizeof(*tp));
    ASSERT(tp, "calloc(1,%zu) failed", sizeof(*tp));
    // We reused the "name" variable, and now it's got the const compiler
    // error; so we use it with a C cast.  The compiled code will still
    // work fine; and without adding another variable.
    tp->name = (char *) name;

    CHECK(pthread_mutex_init(&tp->mutex, 0));
    CHECK(pthread_cond_init(&tp->cond, 0));
    CHECK(pthread_cond_init(&tp->wakerCond, 0));

    tp->graph = g;
    tp->maxThreads = maxThreads;
    // if we have no blocks assigned yet we have a minimum of 1
    tp->maxThreadsRun = 1;

    // Add this thread pool, tp, to the graphs lists.
    ASSERT(qsDictionaryInsert(g->threadPools, name, tp, 0) == 0);

    ++g->numThreadPools;

    // Add this thread pool, tp, to the graph's threadPoolStack at the
    // top.
    if(g->threadPoolStack) {
        DASSERT(!g->threadPoolStack->prev);
        tp->next = g->threadPoolStack;
        g->threadPoolStack->prev = tp;
    }
    g->threadPoolStack = tp;

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));

    return tp;
}


struct QsThreadPool *qsGraph_createThreadPool(struct QsGraph *g,
        uint32_t maxThreads, const char *name) {

    NotWorkerThread();
    DASSERT(g);
    ASSERT(maxThreads > 0);

    CHECK(pthread_mutex_lock(&g->mutex));

    struct QsThreadPool *tp = _qsGraph_createThreadPool(g,
            maxThreads, name);

    if(g->isHalted) {
        // There is a graph wide thread pool halt in effect; so we add an
        // extra uncounted thread pool halt lock now so that this new
        // thread pool does not process any jobs with a worker thread.
        qsGraph_threadPoolHaltLock(g, 0);
        // When the graph wide thread pool halt is turned off, we will
        // undo all the extra thread pool halts.
        //
        // Welcome to recursive lock hell!
    }

    LaunchWorker(tp);

    CHECK(pthread_mutex_unlock(&g->mutex));
    
    return tp;
}


struct QsThreadPool *qsGraph_getThreadPool(struct QsGraph *g,
        const char *name) {

    NotWorkerThread();
    ASSERT(name && name[0]);
    DASSERT(g);
    struct QsThreadPool *tp = 0;

    CHECK(pthread_mutex_lock(&g->mutex));
    DASSERT(g->numThreadPools);

    if(!name || !name[0]) {
        // return the default thread pool.
        tp = g->threadPoolStack;
        // graphs that exist have at least one thread pool.
        DASSERT(tp);
        CHECK(pthread_mutex_unlock(&g->mutex));
        return tp;
    }

    tp = qsDictionaryFind(g->threadPools, name);
    CHECK(pthread_mutex_unlock(&g->mutex));

    return tp; // We may or may not have a return value.
}


void qsGraph_setDefaultThreadPool(struct QsGraph *g,
        struct QsThreadPool *tp) {

    DASSERT(g);
    ASSERT(tp);

    CHECK(pthread_mutex_lock(&g->mutex));

    // This thread pool, tp, must be in this graph.
    ASSERT(tp->graph == g);
    // There must be at least one in the graph thread pool stack list.
    DASSERT(g->threadPoolStack);

    // Move this thread pool, tp, to the top of the stack,
    // at g->threadPoolStack.

    DASSERT(!g->threadPoolStack->prev);

    if(tp == g->threadPoolStack) {
        // It's already at the top, the default.
        DASSERT(!tp->prev);
        goto finish;
    }

    // Okay, move it to the top of the stack, which is the default thread
    // pool.
 
    DASSERT(tp->prev);

    // A possible 6 pointer changes for this threadPoolStack list:
    tp->prev->next = tp->next;
    if(tp->next)
        tp->next->prev = tp->prev;
    tp->next = g->threadPoolStack;
    g->threadPoolStack->prev = tp;
    g->threadPoolStack = tp;
    tp->prev = 0;


finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
}
