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



// We will have a thread pool (graph) mutex before calling this and after.
// When the users trigger callback is called the mutex will be unlocked.
//
// Returns true if the trigger changed its' run/call state and it is a
// source trigger, and false if not.
//
static inline
bool CallTriggerCallback(struct QsTrigger *t, struct QsThreadPool *tp) {


    if(!t->keepLockInCallback)
        // For this case the callback will not unlock the mutex.
        CHECK(pthread_mutex_unlock(tp->mutex));

    DASSERT(pthread_getspecific(_qsGraphKey) == 0);
    CHECK(pthread_setspecific(_qsGraphKey, t->block));
    // This is the call of a function in a simple block module or
    // a wrapper that calls it.
    int ret = t->callback(t->userData);
    CHECK(pthread_setspecific(_qsGraphKey, 0));

    if(!t->keepLockInCallback)
        // For this case the callback will not lock the mutex.
        CHECK(pthread_mutex_lock(tp->mutex));

    // The block can change the trigger from this return value, ret.
    if(ret == 0)
        // No change.  Continue to queue up and call the trigger
        // callback.
        return false; // no trigger change.


    // This trigger did change it's running state.

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

    // Return the answer to the question: Was the trigger was removed from
    // play, at least for this run, and is the trigger a source trigger?
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
// returns true if there is work to do, else returns false.
//
static inline
bool WaitForWork(struct QsThreadPool *tp) {

    DASSERT(tp);

    if(tp->doneRunning) {
        DASSERT(tp->first == 0);
        // Some worker thread in this pool has determined that we are done
        // running for this run.
        return false;
    }

    struct QsGraph *graph = tp->graph;
    DASSERT(graph);


    bool haveSigTrigger =
        sig // there is one
        && sig->trigger.isRunning // it's in use now
        // It's using this thread pool
        && sig->trigger.block->threadPool == tp
        // Another worker thread does not have the job to
        // handle the signal trigger
        && sig->aboutToPause == 0;


    if(haveSigTrigger) {

        // This is major tricky shit with jumps.  Signals are tricky to
        // deal with if you want robust code, with no race conditions.  We
        // can't ever fix over-run conditions (on slow computers), but we
        // can fix race conditions.  There is a difference between an
        // over-run condition and a race condition.

        // Do a check first to avoid the system call below in sigsetjmp().
        if(CheckAndQueueTrigger((struct QsTrigger *) sig))
            // Okay cool, we got a signal and we have a job to work on.
            // There is no need to pause/wait.
            return tp->first;

        // It's important to note: this block of code cannot be made into
        // a separate function because we cannot let said function to get
        // popped from the current function call stack.  We need this code
        // block to exist so long as we have the jump point until the
        // jump flag sig->aboutToPause is unset.

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

        if(sigsetjmp(sig->jmpEnv, 1/*save sigs*/)) {

            // CAUTION: this is a "jump to" point in the code.  The
            // execution flow has a jump in it.

            // We jumped from the signal handler SigAction() to here.
            // Therefore, we must have caught a signal in SigAction() in
            // file trigger.c.

            // Reset the flag to signal handler that we have set the jump
            // point.
            sig->aboutToPause = 0;

            // Here's to hoping that we have the mutex lock at this point.
            // ... testing ... Looks like the pthreads, (NPTL) Native
            // POSIX Threads Library, takes care of making it so that
            // jumping out of a pthread_cond_wait() call keeps things
            // consistent.  Wow, it's fuck'n magic.
            //
            DASSERT(sig->triggered);
            ASSERT(CheckAndQueueTrigger((struct QsTrigger *) sig));
            --graph->numIdleThreads;
            --tp->numIdleThreads;

            DSPEW("ThreadPool has %" PRIu32 " idle threads out of %"
                    PRIu32,
                    tp->numIdleThreads, tp->numThreads);

            return tp->first;
        }

        // Now we can incremented the numIdleThreads counter because we
        // will not, and can not, jump until the sig->aboutToPause flag is
        // set just below.  Incrementing the numIdleThreads counter after
        // sig->aboutToPause is set would be a disaster.
        ++graph->numIdleThreads;
        ++tp->numIdleThreads;

        // What thread may do this jump.
        sig->thread = pthread_self();

        // Flag to signal handler that we have set the jump point and we
        // are ready to use it.  This is the only point in all the code
        // where this flag is set.  This must be set with care at the
        // correct line of code, after some stuff and before other stuff.
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
            // We still have the mutex lock, so no one will know that we
            // falsely incremented the numIdleThreads counter.  So it
            // looks like we never were idle.
            --graph->numIdleThreads;
            --tp->numIdleThreads;

            return tp->first;
        }

    } else {
        // There is no jump set, hence this will not get fucked by
        // incrementing this counter.
        ++graph->numIdleThreads;
        ++tp->numIdleThreads;
    }

    DSPEW("ThreadPool has %" PRIu32 " idle threads out of %" PRIu32,
            tp->numIdleThreads, tp->numThreads);

    // We cannot set --graph->numIdleThreads in this "jump from zone";
    // that would make it not possible to know if it was decremented,
    // because we will not know if we jumped from before or after the time
    // it jumps.


    ////////////////////////////////////////////////////////////////////
    // This next function call needs to be a blocking system call:
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
        // Flag to signal trigger's signal handler not to jump now.
        sig->aboutToPause = 0;
        // Check if we had a signal event for this sig trigger.
        CheckAndQueueTrigger((struct QsTrigger *) sig);
    }

    --graph->numIdleThreads;
    --tp->numIdleThreads;

    DSPEW("ThreadPool has %" PRIu32 " idle threads out of %" PRIu32,
            tp->numIdleThreads, tp->numThreads);


    return tp->first;
}


static void *runWorker(struct QsThreadPool *tp);


// We need the graph/threadPool mutex locked to call this.
static
void LaunchWorkerThread(struct QsThreadPool *tp) {

    CHECK(pthread_create(&(tp->threads[tp->numThreads].thread), 0,
                (void* (*)(void *)) runWorker, tp));
    ++tp->graph->numWorkingThreads;
    DSPEW("There are now %" PRIu32 " worker threads",
            tp->graph->numWorkingThreads);
    // Mark this thread to be joined when we finish running.
    tp->threads[tp->numThreads].hasLaunched = true;
    ++tp->numThreads;
}


// Each worker thread will call this:
//
static
void *runWorker(struct QsThreadPool *tp) {

    struct QsGraph *graph = tp->graph;

    CHECK(pthread_mutex_lock(tp->mutex));


    while(tp->first || WaitForWork(tp)) {

        // If WaitForWork() kept us here, then there must be work in the
        // thread pool queue for at least one of the blocks.
        DASSERT(tp->first);
        DASSERT(tp->last);

        struct QsSimpleBlock *b;
        // If WaitForWork() kept us here, then there must be work in the
        // thread pool queue for at least one of the blocks.

        bool sourceTriggersChanged = false;

        if(tp->first->next) {
            // We have even more work to do.
            if(tp->numIdleThreads)
                // wake an idle thread.
                //
                // man pthread_cond_signal says: If several threads are
                // waiting on cond, exactly one is restarted, but it is
                // not specified which.  So this should wake just one,
                // very cool.
                CHECK(pthread_cond_signal(&tp->cond));

            else if(tp->numThreads < tp->maxThreads)
                // Launch another worker thread.
                LaunchWorkerThread(tp);

            // Else we are at our limit of worker threads.
        }


        while(tp->first) {

            // Loop over blocks:
            //
            b = PopBlockFromThreadPoolQueue(tp);
            // We call callbacks for this block, b:
            DASSERT(b);

            while(b->firstJob) {

                // This thread is now dedicated to finishing all jobs in
                // the b->firstJob queue.
                b->busy = true;
                // Loop over triggers in the block
                //
                struct QsTrigger *t = PopJobBackToTriggers(b);
                DASSERT(t);

                // This is where we work for the block
                if(CallTriggerCallback(t, tp))
                    sourceTriggersChanged = true;

            } // end loop over triggers in block

            // All the jobs for this block are done, so now any worker
            // thread can queue the block into the threadPool queue
            // again.
            b->busy = false;

        }// end loop over blocks in thread pool


        // At this point there are no triggered jobs in this thread pool
        // queue.  So for whatever reason this thread pool needs to wait
        // for work.

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
                if(tp->maxThreads == 0 || tp->graph->threadPools->next == 0) {
                    // Do not have a usable source trigger in this thread
                    // pool and we have no queued jobs, and we only have
                    // the main thread running this or just one thread
                    // pool.
                    //
                    break;
                } else
                    ASSERT(0, "WRITE THIS CODE");
                // TODO:  More CODE HEREEEEEEEEE

            }

        }

    } // while(tp->first || WaitForWork(tp)) { // loop


    if(!tp->doneRunning) {
        DASSERT(tp->first == 0);
        // This will let all threads in the pool that we have determined
        // that there will be no more work for this thread pool.
        tp->doneRunning = true;
    }

    if(tp->maxThreads == 0) {
        // this is the case of the main thread running the flow stream.
        // And so in a sense there are no worker threads.
        CHECK(pthread_mutex_unlock(tp->mutex));
        return 0;
    }

    // This thread is no longer a working thread so un-count it.
    --graph->numWorkingThreads;

    if(graph->numWorkingThreads == 0 && graph->masterWaiting)
        // Last one out signal the main thread.
        CHECK(pthread_cond_signal(&graph->cond));
    else if(tp->numIdleThreads) {
        ERROR("                         SIGNALING tp COND");
        CHECK(pthread_cond_broadcast(&tp->cond));
    }

    DSPEW("Thread exiting graph numWorkingThreads=%" PRIu32
            "  tp numIdleThreads=%" PRIu32,
        graph->numWorkingThreads, tp->numIdleThreads);

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
        // Triggers can move between the 2 lists: smB->waiting, and
        // smB->firstJob
        //
        //    smB->waiting = triggers not queued now
        //    smB->firstJob = triggers queued now
        //
        struct QsTrigger *triggers[] = { smB->waiting, smB->firstJob};
        int i = 0;
        for(; i < 2 ; ++i) {
            struct QsTrigger *t = triggers[i];
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
        if(i<2)
            // We have at least one source trigger to run.
            break;
        // Else we looped through all triggers in this block, b, and found
        // no sources that can run.
    }
    if(!b) {
        // We looped through all triggers in all blocks and found no
        // sources that can run.
        NOTICE("No source triggers found, nothing to run");
        // There is nothing to run.
        return 1;
    }


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
        DASSERT(graph->numWorkingThreads == 0);
        DASSERT(graph->numIdleThreads == 0);
        // We start by launching one worker thread per thread pool.  The
        // worker threads will add more worker threads as they are needed
        // up to tp->maxThreads threads per thread pool.
        for(struct QsThreadPool *tp = graph->threadPools; tp;
                tp = tp->next) {
            DASSERT(tp->threads);
            DASSERT(tp->maxThreads);
            DASSERT(tp->numThreads == 0);
            DASSERT(tp->numIdleThreads == 0);

            LaunchWorkerThread(tp);
        }
        CHECK(pthread_mutex_unlock(&graph->mutex));
    }

    return 0;
}
