#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "block.h"
#include "graph.h"
#include "threadPool.h"
#include "builder.h"
#include "trigger.h"
#include "triggerJobsLists.h"
#include "run.h"




// We will have a thread pool mutex before calling this and after.  When
// the users trigger callback is called the mutex will be unlocked.
//
// Returns true if the trigger changed its' run/call state and it is a
// source trigger, and false if not.
//
static inline
bool CallTriggerCallback(struct QsTrigger *t, struct QsThreadPool *tp) {


    // TODO: The callback() may need to be called more than once,
    // which is why we made this function.

    CHECK(pthread_mutex_unlock(tp->mutex));
    // This is the call of a function in a simple block module.
    int ret = t->callback(t->userData);
    CHECK(pthread_mutex_lock(tp->mutex));

    // The block can change the trigger from this return value, ret.
    if(ret == 0)
        // No change.  Continue to queue up and call the trigger
        // callback.
        return false; // no trigger change.

    if(ret > 0)
        // Remove the trigger callback for the rest of the run/flow
        // cycle.
        //
        TriggerStop(t);
    else
        // ret < 0
        //
        // Remove the trigger from all runs in this graph.  This will
        // destroy the trigger.
        FreeTrigger(t);

    // Return that the trigger was removed from play at least for this
    // run, but only if it's a source trigger.
    return t->isSource;
}



// TODO: we may want to optimize this by having different versions of this
// Pause function that are selected before now.  For now this in fine
// until we learn all the Pause() cases that are needed.  Cases so far are
// with signal trigger and without, and with one thread and with more than
// one thread.
//
// We have a thread pool mutex lock when calling this.  It may unlock
// inside, if it really pauses, and then lock before returning.
//
// Hence this is called with a thread pool mutex lock and returns with a
// thread pool mutex lock.
//
static inline
bool WaitForWork(struct QsThreadPool *tp) {


    bool haveSigTrigger =
        sig // there is one
        && sig->trigger.isRunning // it's in use now
        // it's using this thread pool
        && sig->trigger.block->threadPool == tp;


    if(haveSigTrigger) {

        // Tricky shit with jumps.  Signals are tricky to deal with if you
        // want robust code, with no race conditions.  We can't ever fix
        // over-run conditions (on slow computers), but we can fix race
        // conditions.

        DASSERT(sig->aboutToPause == 0);

        // Do a check first to avoid the system call below in sigsetjmp().
        if(CheckAndQueueTrigger((struct QsTrigger *) sig))
            // Okay cool, we got a signal and we have a job to work on.
            // There is no need to pause/wait.
            return tp->first;

        // It's important to note: this block of code cannot be made
        // into a separate function because we cannot let said function to
        // get popped from the current function stack.  We need this code
        // block to exist so long as we have the jump point until the jump
        // flag sig->aboutToPause is unset.

        // I think that sigsetjmp(3) and siglongjmp(3) do not usually
        // cause system calls; they just muck with the program memory and
        // the execution pointer, or whatever that crap is called.  The
        // man # 3 means they are not directly system call wrappers, but
        // that does not mean they don't necessarily make a system call.
        // I'll write a test, tests/setjmp.c and see using strace:
        //
        //       cd ../tests; make; strace ./setjmp
        //
        // and, shit, I see there is a system call:
        //
        //    rt_sigprocmask(SIG_SETMASK, [], NULL, 8) = 0
        //
        // is called from sigsetjmp() and another test version shows that
        // setjmp() does not make a system call, which makes sense why
        // they have the two flavors of *setjmp() and *longjmp().
        //
        // Tests show that we need to use sigsetjmp(), and setjmp() will
        // not work.  Shit happens.
        //

        if(sigsetjmp(sig->jmpEnv, 1/*save sigs*/)) {

            // We jumped from the signal handler SigAction() to here.
            // Therefore, we must have caught a signal in SigAction() in
            // file trigger.c.

            // Reset the flag to signal handler that we have set the jump
            // point.
            sig->aboutToPause = 0;

            // Here's to hoping that we have the mutex lock at this point.
            // ... testing ... Looks like the kernel takes care of making
            // it so that jumping out of a pthread_cond_wait() call keeps
            // things consistent.  Wow, it's fuck'n magic.
            //
            DASSERT(sig->triggered);
            ASSERT(CheckAndQueueTrigger((struct QsTrigger *) sig));
            return tp->first;
        }
        // Flag to signal handler that we have set the jump point and we
        // are ready to use it.
        sig->aboutToPause = 1;
        //
        // We need to check if we got a signal just before we where
        // setting the flag above.  This fixes a race condition.
        if(CheckAndQueueTrigger((struct QsTrigger *) sig)) {
            // We have a signal event to respond to.  If we got more than
            // one signal that's just an over-run condition; the computer
            // runs to slowly and there's nothing that can fix that but a
            // faster computer.
            //
            // Set flag to disable the siglongjmp() call in function
            // SigAction() in file trigger.c.
            sig->aboutToPause = 0;
            return tp->first;
        }
    }


    ////////////////////////////////////////////////////////////////////
    // This next function call needs to be blocking system call:
    ////////////////////////////////////////////////////////////////////
    //
    // 0. pause(2) works if there is just one thread running this
    //
    // 1. pthread_cond_wait(3) if we have no file descriptors to wait on
    //
    // 2. epoll_wait(2) if we have file descriptors to wait on
    //

    // If this call is in error due at the signal that we catch we will
    // not see the error, because our signal catcher jumps out of that
    // call to a sigsetjmp() above.  Without this jumping complexity there
    // is a terrible race condition, and signals could be ignored when
    // they should not, and we could have the program stuck in this next
    // function call forever.  The fix is a jump in the signal handler.

    // pause/wait.  Mutex unlock inside pthread_cond_wait()
    CHECK(pthread_cond_wait(&tp->cond, tp->mutex));
    //pause();


    if(haveSigTrigger) {
        // Flag to signal trigger's signal handler not to jump.
        sig->aboutToPause = 0;
        // Check if we had a signal event for this sig trigger.
        CheckAndQueueTrigger((struct QsTrigger *) sig);
    }

    return tp->first;
}


// Each worker thread will call this:
//
static
void *runWorker(struct QsThreadPool *tp) {


    CHECK(pthread_mutex_lock(tp->mutex));


    while(WaitForWork(tp)) {
        struct QsSimpleBlock *b;
        // If WaitForWork() kept us here, then there must be work
        // in the thread pool queue of blocks:
        DASSERT(tp->first);
        DASSERT(tp->last);
        bool sourceTriggersChanged = false;

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

                if(CallTriggerCallback(t, tp))
                    sourceTriggersChanged = true;

            } while(b->firstJob); // end loop over triggers in block

        } while(tp->first); // end loop over simple blocks in thread pool


        // At this point there are no triggered jobs in this thread pool.

        if(sourceTriggersChanged) {
            // Assess what will WaitForWork() should do now.  This is a
            // transient case, so the below loops do not happen often.
            //
            // Hence, check if we have any running source triggers in this
            // pool.
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
            if(!b) {
                if(tp->maxThreads == 0 || tp->graph->threadPools->next == 0)
                    // Do not have a usable source trigger in this thread
                    // pool and we have no queued jobs, and we only have
                    // the main thread running this or just one thread
                    // pool.
                    //
                    break;

                // TODO:  More CODE HEREEEEEEEEE

            }

        }

    } // while(WaitForWork(tp)) loop


    if(tp->maxThreads == 0)
        CHECK(pthread_mutex_unlock(tp->mutex));
        return 0;



    //--tp->graph->numWorkingThreads


    CHECK(pthread_mutex_unlock(tp->mutex));


    return 0;
}


// Returns 0 if it runs and 1 if not.
//
int run(struct QsGraph *graph) {

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
        NOTICE("No source triggers found, nothing to run");
        // There is nothing to run.
        return 1;
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

    // TODO: We could not initialize and use the mutex and conditionals
    // when we run with the main thread.

    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next) {
        tp->mutex = &graph->mutex;
        CHECK(pthread_cond_init(&tp->cond, 0));
    }

    if(graph->threadPools->maxThreads == 0) {
        // This is the only thread pool and it has no threads.  This is a
        // special case of running with the main thread and returning when
        // finished or signaled.
        DASSERT(graph->threadPools->next == 0);

        // The main thread will run the stream.
        runWorker(graph->threadPools);

    } else {

        CHECK(pthread_mutex_lock(&graph->mutex));
        // We start by launching one worker thread per thread pool.  The
        // worker threads will add more worker threads as they are needed
        // up to tp->maxThreads threads per thread pool.
        for(struct QsThreadPool *tp = graph->threadPools; tp;
                tp = tp->next) {
            DASSERT(tp->threads);
            DASSERT(tp->maxThreads);
            DASSERT(tp->numThreads == 0);
            tp->numThreads = 1;
            // Launch a worker thread:
            CHECK(pthread_create(&tp->threads->thread, 0,
                        (void* (*)(void *)) runWorker, tp));
            tp->threads->hasLaunched = true;
        }
        CHECK(pthread_mutex_unlock(&graph->mutex));
    }

    return 0;
}
