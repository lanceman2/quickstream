#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
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
#include "job.h"



// Called from job->work().  Since we cannot call
// qsGraph_threadPoolHaltLock() from in a work() call, we make this
// asynchronous.  The 
//
void qsJob_threadPoolHaltLockAsync(struct QsJob *j,
        void (*callback)(struct QsJob *j)) {
}



static inline
void SetHalt(struct QsThreadPool *tp,
                struct QsGraph *g) {
    DASSERT(g);
    DASSERT(tp);

    CHECK(pthread_mutex_lock(&tp->mutex));

    if(tp->halt) {
        // The thread pool halt flag is set so if we are calling
        // this this thread pool, tp, must be halted already; which
        // is okay because this thread pool halt lock thing is
        // recursive.
        DASSERT(!tp->numWorkingThreads);
        DASSERT(!tp->signalingOne);
        // Don't let any worker threads out from the pthread_cond_wait()
        // loop.
        goto finish;
    }

    tp->halt = true;
    tp->signalingOne = false;

finish:
    CHECK(pthread_mutex_unlock(&tp->mutex));
}


static inline
void WaitHalt(struct QsThreadPool *tp,
                struct QsGraph *g) {
    DASSERT(g);
    DASSERT(tp);

    CHECK(pthread_mutex_lock(&tp->mutex));

    DASSERT(tp->halt);
    DASSERT(!tp->signalingOne);

    if(tp->numWorkingThreads) {
        // This is cool not having to put this condition variable in a
        // quickstream struct.
        pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
        // Tell the worker loop to signal this thread
        // when they go to wait/sleep.
        DASSERT(tp->haltCond == 0);
        tp->haltCond = &cond;
        CHECK(pthread_cond_wait(&cond, &tp->mutex));
        DASSERT(tp->numWorkingThreads == 0);
        tp->haltCond = 0;
    } // else it's halted already.

    CHECK(pthread_mutex_unlock(&tp->mutex));
}


// This is a recursive lock thingy.  You can call it N times and then you
// can call qsGraph_threadPoolHaltUnlock() N times.  To unlock it you call
// qsGraph_threadPoolHaltUnlock() N times to unlock on the N-th call.
//
void qsGraph_threadPoolHaltLock(struct QsGraph *g,
        struct QsThreadPool *tp) {

    // Only non-thread-pool-worker threads may get a thread pool halt
    // lock.
    NotWorkerThread();
    DASSERT(g);

    // We will hold this lock until g->haltCount goes to zero in the last
    // corresponding qsGraph_threadPoolHaltUnlock(g) call.
    CHECK(pthread_mutex_lock(&g->mutex));

    // When the graph is destroyed there is a time when there is no
    // thread pool in it and g->threadPools == 0
    //
    // So we'll test that we do not call this with a bad non-zero thread
    // pool pointer.
    DASSERT(g->threadPools || !tp);

    ++g->haltCount;

    if(!g->threadPools) {
        CHECK(pthread_mutex_unlock(&g->mutex));
        // The graph is being destroyed.
        return;
    }

    if(!tp) {
        // Halt all thread pools in the graph.
        //
        // If there are a few thread pools it's faster to
        // first tell all the thread pools to go to halt:
        for(struct QsThreadPool *p = g->threadPoolStack;
                p; p = p->next)
            SetHalt(p, g);
        // and then wait for them to signal us if they
        // have not halted already:
        for(struct QsThreadPool *p = g->threadPoolStack;
                p; p = p->next)
            WaitHalt(p, g);
        // This way the thread pools can go to "halt" in parallel,
        // and we only wait for the slower ones.
    } else {

        // Halt just this thread pool, tp.
        SetHalt(tp, g);
        WaitHalt(tp, g);
    }
}


static inline
void CheckUnhaltThreadPool(struct QsThreadPool *tp,
                struct QsGraph *g) {
    DASSERT(g);
    DASSERT(tp);

    CHECK(pthread_mutex_lock(&tp->mutex));

    if(!tp->halt)
        // This thread pool was not halted to begin with.
        goto finish;

    tp->halt = false;
    DASSERT(!tp->signalingOne);

    DASSERT(tp->numWorkingThreads == 0);

    if(tp->numThreads == 0)
        goto finish;

    // Wake Up a thread pool worker
    tp->signalingOne = true;
    CHECK(pthread_cond_signal(&tp->cond));

finish:

    CHECK(pthread_mutex_unlock(&tp->mutex));
}


void qsGraph_threadPoolHaltUnlock(struct QsGraph *g) {

    NotWorkerThread();
    DASSERT(g);

    // Note this is a recursive "halt" lock thingy.  We must unlock it the
    // same number of times it was locked by calling
    // qsGraph_threadPoolHaltLock().

    // All the thread pools stay halted until the counter g->haltCount
    // goes to 0.  This is unlocked only at the last matching
    // qsGraph_threadPoolHaltUnlock() call.

    --g->haltCount;

    if(!g->haltCount && g->threadPools)
        for(struct QsThreadPool *p = g->threadPoolStack;
                p; p = p->next)
            CheckUnhaltThreadPool(p, g);

    // g->mutex is a recursive mutex.  It can get unlocked many times,
    // but only the "corresponding last one" will really unlocks it.
    CHECK(pthread_mutex_unlock(&g->mutex));
}
