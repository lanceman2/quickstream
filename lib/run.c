#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "trigger.h"
#include "graph.h"
#include "threadPool.h"
#include "triggerJobsLists.h"
#include "builder.h"
#include "run.h"


#if 0
static inline
bool IfNeedMutex(struct QsThreadPool *tp) {
    return (tp->maxThreads > 1);
}
#endif


static inline
void MutexLock(struct QsThreadPool *tp) {

    CHECK(pthread_mutex_lock(&tp->mutex));
}


static inline
void MutexUnlock(struct QsThreadPool *tp) {

    CHECK(pthread_mutex_unlock(&tp->mutex));
}


// We have a thread pool mutex lock when calling this.
static inline
void Pause(struct QsThreadPool *tp) {

    // Tricky shit with jumps.  Signals are tricky to deal with if you
    // want robust code, with no race conditions.

    if(sig && sig->trigger.isRunning) {

        DASSERT(sig->aboutToPause == 0);

        if(CheckAndQueueTrigger((struct QsTrigger *) sig))
            // We have a signal event to respond to.
            return;

        // It's important to note: this block of code cannot be made
        // into a separate function because we cannot let said function to
        // get popped from the current function stack.  We need this code
        // block to exist so long as we have the jump point until the jump
        // flag sig->aboutToPause is unset.

        if(sigsetjmp(sig->jmpEnv, 1/*save sigs*/)) {

            // We jumped from the signal handler SigAction() to here.
            // Therefore we must have caught a signal in SigAction() in
            // file trigger.c.

            // Reset the flag to signal handler that we have set the jump
            // point.
            sig->aboutToPause = 0;

            // Here's to hoping that we have the mutex lock at this
            // point.
            //
            DASSERT(sig->triggered);
            ASSERT(CheckAndQueueTrigger((struct QsTrigger *) sig));
            return;
        }
        // Flag to signal handler that we have set the jump point.
        sig->aboutToPause = 1;
    }


    /////////////////////////////////////////////////////////////////////
    // This function needs to wait on the correct type of blocking call:
    /////////////////////////////////////////////////////////////////////
    //
    // 1. pthread_cond_wait() if we have no file descriptors to wait on
    //
    // 2. epoll_wait() if we have file descriptors to wait on
    //

    // If this call is in error due at the signal that we catch we will
    // not see the error, because our signal catcher jumps out of that
    // call to a setjmp() in the above.  Without this jumping complexity
    // there is a terrible race condition, and signals could be ignored
    // when they should not.

    // pause/wait.  Mutex unlock inside pthread_cond_wait()
    //CHECK(pthread_cond_wait(&tp->cond, &tp->mutex));

    pause();

    if(sig && sig->trigger.isRunning) {
        // Flag to signal trigger's signal handler not to jump.
        sig->aboutToPause = 0;
        // Check if we had a signal event for this sig trigger.
        CheckAndQueueTrigger((struct QsTrigger *) sig);
    }
}


// We need the threadPool mutex lock before calling this.
//
// This is the blocking call for the worker threads when there is only a
// system signal trigger, and no other triggers.  So there is only
// one simple block with just one trigger, this signal trigger, sig.
// Otherwise we could use a different blocking call than pause(2) which we
// use here.
//
// Return true if we have work or false if the graph is done running.
//
static inline
bool CheckForWork(struct QsThreadPool *tp) {

    DASSERT(tp);

    if(tp->first)
        // We already have work.
        return true;

    Pause(tp);

    return tp->first;
}


// Do not fucking break up this function making it hard to follow what the
// fuck it is doing.  Code that shows what the fuck it is doing is good
// code.  Breaking up code into small functions sucks my ass.  That
// bullshit they teach at Universities is just that, bullshit.
//
// We wish to keep this code followable.  Scope-able code.
//
// Each worker thread will call this:
//
static
void *runWorker(struct QsGraph *g, struct QsThreadPool *tp) {

    MutexLock(tp);

    while(CheckForWork(tp)) {
        struct QsSimpleBlock *b;
        // If CheckForWork() kept us here, then there must be work
        // in the thread pool queue of blocks:
        DASSERT(tp->first);
        DASSERT(tp->last);
        bool triggersChanged = false;

        do {
            // Loop over blocks:
            //
            b = PopBlockFromThreadPoolQueue(tp);
            // We call callbacks for this block, b:
            DASSERT(b);
            // ... and there must be at least one trigger that we can pop.
            DASSERT(b->firstJob);

            do {
                // Loop over triggers in the block
                //
                struct QsTrigger *t = PopJobBackToTriggers(b);
                DASSERT(t);

                MutexUnlock(tp);
                int ret = t->callback(t->userData);
                MutexLock(tp);

                // The block can change the trigger from this return
                // value, ret.
                if(ret == 0) continue;

                if(ret > 0)
                    // Remove the trigger callback for the rest of the
                    // run.
                    //
                    TriggerStop(t);
                else
                    // ret < 0
                    //
                    // Remove the trigger from all runs.
                    FreeTrigger(t);

                if(!triggersChanged)
                    triggersChanged = true;

            } while(b->firstJob); // end loop over triggers

        } while(tp->last); // end loop over simple blocks


        // At this point there are no triggered jobs.

        if(triggersChanged) {
            // Assess what will CheckForWork() should do now.  This is a
            // transient case, so the below loops do not happen often.
            //
            // TODO: If triggers every have the ability to be "turned on"
            // by setting QsTrigger::isRunning at run/flow time then we'll
            // need to make this looping below faster by adding more
            // stupid lists to the data structures.
            //
            // The thread pool does not have a list of all blocks that it
            // services, so we use the block list in the graph to search
            // all blocks and find ones that use this thread pool.
            struct QsBlock *b = tp->graph->firstBlock;
            for(; b; b = b->next) {
                if(b->isSuperBlock) continue;
                struct QsSimpleBlock *smB = (struct QsSimpleBlock *)b;
                if(smB->threadPool != tp) continue;
                struct QsTrigger *t = smB->waiting;
                for(; t; t = t->next)
                    if(t->isRunning && t->isSource)
                        // got a usable source trigger
                        break;
                if(t)
                    // got a usable source trigger
                    break;
            }
            if(!b)
                // Do not have a usable source trigger.  Break from the
                // main run loop.  We are finished running.
                break;

            // else got a usable source trigger, so we keep looping the
            // main loop.
        }

    } // while(CheckForWork(tp)) loop

    MutexUnlock(tp);

    return 0;
}


void run(struct QsGraph *graph) {

    DASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph->threadPools);
    DASSERT(graph->flowState == QsGraphFlowing);

    // Find a block with a trigger that can be run.
    struct QsBlock *b = graph->firstBlock;
    for(; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        // There should be no triggered jobs yet.
        DASSERT(smB->firstJob == 0);
        DASSERT(smB->lastJob == 0);
        struct QsTrigger *t = smB->waiting;
        for(; t; t = t->next) {
            DASSERT(t->block == smB);
            if(t->isRunning && t->isSource)
                // We have at least one source trigger to run.
                break;
        }
        if(t)
            // We have at least one source trigger to run.
            break;
    }
    if(!b) {
        NOTICE("No stream triggers found, nothing to run");
        // There is nothing to run.
        return;
    }

    // First queue up the triggered triggers in the into their thread
    // pool.  Un-triggered triggers stay in the "waiting" trigger list in
    // their blocks.
    //
    b = graph->firstBlock;
    for(; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsTrigger *t = ((struct QsSimpleBlock *) b)->waiting;
        if(t) {
            // This simple block, b, has at least one trigger:
            //
            struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
            DASSERT(t->block == smB);
            while(t) {
                // We are editing the trigger lists as we iterate through
                // them, hence we get the next before we move the current
                // "t" to the queue.
                struct QsTrigger *nextT = t->next;
                if(t->isRunning)
                    // This will do queue a job if the trigger triggered.
                    CheckAndQueueTrigger(t);
                t = nextT;
            }
        }
    }


    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next) {
        CHECK(pthread_mutex_init(&tp->mutex, 0));
        CHECK(pthread_cond_init(&tp->cond, 0));
    }

    if(graph->threadPools->maxThreads == 0) {
        // This is the only thread pool and it has no threads.  This is a
        // special case of running with the main thread and returning when
        // finished or signaled.
        DASSERT(graph->threadPools->next == 0);

        // The main thread will run the stream.
        runWorker(graph, graph->threadPools);

    } else {

        // TODO: HERE initialize each threadPool mutexs and conditionals.


        // TODO: HERE We will have worker threads.


        ERROR("Write MORE CODE HERE");
    }

    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next) {
        CHECK(pthread_mutex_destroy(&tp->mutex));
        CHECK(pthread_cond_destroy(&tp->cond));
    }
}
