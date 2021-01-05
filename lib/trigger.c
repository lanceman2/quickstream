#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "debug.h"
#include "parameter.h"
#include "trigger.h"
#include "block.h"
#include "threadPool.h"
#include "triggerJobsLists.h"
#include "graph.h"
#include "run.h"


// This checks if the trigger fired and queues up the job for the block.
//
// We must have the graph->mutex lock to call this.
//
// It's an error to call this if the trigger is in the job queue.
//
// Returns true if the trigger is turned into a job in the queue and
// false if not.
//
//
// t  is the trigger thingy we wish to queue a job for.
//
// tp is the thread pool that has the thread that called this function.
//    If tp is not the same thread pool that the trigger belongs to we
//    need to let that other thread pool know.
//
//
bool CheckAndQueueTrigger(struct QsTrigger *t,
        struct QsThreadPool *tp) {

    DASSERT(t);
    DASSERT(t->isInJobQueue == false);
    DASSERT(t->isRunning, "Block %s t->kind=%d",
            t->block->block.name, t->kind);

    // First check
    if(t->checkTrigger && !t->checkTrigger(t->userData)) return false;


    // Now Queue it
    if(t->kind < QsStreamSource)
        WaitingToFirstJob(t);
    else
        WaitingToLastJob(t);


    if(tp != t->block->threadPool) {
        // The thread that is queuing this job is not in the same
        // threadPool as the block that owns this trigger.  So, we must
        // check that the threadPool that the block belongs to needs to
        // have an idle worker thread woken from a wait
        // (pthread_cond_wait()) call or launch another thread.
        CheckMakeWorkerThreads(t->block->threadPool);
    }

    return true;
}


void *AllocateTrigger(size_t size, struct QsSimpleBlock *b,
        enum QsTriggerKind kind, int (*callback)(void *userData),
        void *userData, bool (*checkTrigger)(void *userData),
        bool isSource) {

    DASSERT(b);
    DASSERT(b->block.graph);
    //DASSERT(checkTrigger);

    // This is freed in block.c.
    struct QsTrigger *t = calloc(1, size);
    ASSERT(t, "calloc(1,%zu) failed", size);
    t->block = b;
    t->kind = kind;
    t->size = size;
    // Add this to the list of "waiting" in the block.
    if(b->waiting) {
        DASSERT(b->waiting->prev == 0);
        b->waiting->prev = t;
        t->next = b->waiting;
    }
    b->waiting = t;

    t->callback = callback;
    t->userData = userData;
    t->checkTrigger = checkTrigger;
    t->isSource = isSource;

    return (void *) t;
}


void FreeTrigger(struct QsTrigger *t) {

    DASSERT(t);
    struct QsSimpleBlock *b = t->block;
    DASSERT(b);
    DASSERT(!t->isInJobQueue);

    // Stop it if it needs to.
    TriggerStop(t);

    // remove this trigger from the block waiting queue.
    if(t->prev) {
        DASSERT(b->waiting != t);
        t->prev->next = t->next;
    } else {
        DASSERT(b->waiting == t);
        b->waiting = t->next;
    }
    if(t->next)
        t->next->prev = t->prev;

#ifdef DEBUG
    memset(t, 0, t->size);
#endif
    free(t);
}



// TODO: Do we want the ability to make more of these.
//
// We may have just one of these:
struct QsSignal *sig = 0;

//jmp_buf jmpEnv;

static
void SigAction(int signum) {

    DASSERT(sig);

    if(sig->triggered)
        // The signals are coming so fast that we have not acted on the
        // last signal; i.e. we are over-run with signals.
        return;

    if(sig->aboutToPause) {

        if(sig->thread != pthread_self()) {
            // This thread is not the thread that should act on this trigger
            // event.  Send the signal to a particular thread.
            // The thread sig->thread is waiting on a blocking call
            // to get this signal and jump it to acting on it.
            CHECK(pthread_kill(sig->thread, sig->signum));
            return;
        }

        // This is an atomic set:
        sig->triggered = 1;

        // Because AboutToPause is set we are guaranteed that we are in
        // the void Pause(struct QsThreadPool *tp) function from file
        // run.c.

        // I knew that this setjmp() and longjmp() shit could be very
        // handy.  This is so fuck'n coool... Totally fixes a race
        // condition.
        //
        // We may or may not have jumped to this signal handler function
        // from some kind of pause (blocking) system call, it does not
        // matter; we just jump out to where we want to be, which is at
        // the sigsetjmp() from in function Pause() from file run.c.
        //
        // We now jump to before the pause function.  So if we got to
        // running the pause system call (whatever it was) after this call
        // we will be before the pause system call, and we'll see this
        // signal "triggered" as it is now.
        //
        siglongjmp(sig->jmpEnv, 1/*any non zero value*/);
    }
}


static bool
SigCheckTrigger(void *data) {

    DASSERT(sig);

    if(sig->triggered) {
        //
        // If there is a signal now (after the reading of the triggered
        // flag and before this writing of it) then we will not respond to
        // it because the program is running to slowly.  We'll only get
        // the event from the signal before now.  It's just that the
        // computer is too slow to run the users code with the current
        // interval timer setting.  It would be a under-run condition.
        // There's nothing that can fix that except a faster computer.
        //
        sig->triggered = 0;
        return true;
    }

    return false;
}


int qsTriggerSignalCreate(int signum,
        int (*callback)(void *userData),
        void *userData) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    // TODO: Can we make more than one if they use different signal
    // numbers?
    ASSERT(sig == 0, "We already have a QsTriggerSignal");

    // Get the block module that is calling this function:
    struct QsBlock *b = GetBlock();
    ASSERT(b->inWhichCallback == _QS_IN_DECLARE,
            "this must be called in declare()");
    ASSERT(b->isSuperBlock == false, "SuperBlocks may not call this");
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    sig = AllocateTrigger(sizeof(*sig), smB, QsSignal, callback, userData,
            SigCheckTrigger, true/*isSource*/);
    sig->signum = signum;

    return 0; // success
}


// Called in qsGraphRun() before running the flow.
//
// TODO: These TriggerStart() and TriggerStop() functions could be made to
// call start and stop callbacks in the trigger struct.  This makes making
// new triggers have kind-of an inconsistent interface.
//
void TriggerStart(struct QsTrigger *t) {

    DASSERT(t);

    if(t->isRunning) return;
    t->isRunning = true;

    switch(t->kind) {

        case QsSignal:
        {
            // gcc does not let me format this code block in the form that
            // is consistent with the rest of this code.  The ":" requires
            // a newline after it.
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_handler = SigAction;
            CHECK(sigaction(((struct QsSignal *) t)->signum, &act, 0));
        }
        break;
        case QsSetterTrigger:
        {
            // There is a setter that is connected to a 
            struct QsSetter *s = (struct QsSetter *) t->userData;
            s->trigger = t;
            DASSERT(s->parameter.first->kind == QsGetter);
            QueueUpSetterFromGetter(s, s->parameter.first);
        }
        break;
        case QsStreamSource:
        case QsStreamIO:
        break;
    }
}


void TriggerStop(struct QsTrigger *t) {

    DASSERT(t);
    DASSERT(t->isInJobQueue == false);

    if(t->isRunning == false) return;

    switch(t->kind) {

        case QsSignal:
        {
            // gcc does not let me format this code block in the form that
            // is consistent with the rest of this code.  The ":" requires
            // a newline after it.
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_handler = 0;
            CHECK(sigaction(((struct QsSignal *) t)->signum, &act, 0));
        }
        break;
        case QsSetterTrigger:
            ((struct QsSetter *)t->userData)->trigger = 0;
        break;
        case QsStreamSource:
        case QsStreamIO:
        break;
    }

    t->isRunning = false;
}
