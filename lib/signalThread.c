#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "signalThread.h"


#define WAKEUP_SIG  (SIGCONT)


static
struct QsSignalThread *signalThread = 0;

// Protect the qsSignalJobCreate() API for if more than one thread
// call it at once.
static
pthread_mutex_t apiMutex = PTHREAD_MUTEX_INITIALIZER;

// Sync the Sleepy signal job queuing thread with qsSignalJobCreate().
static
pthread_mutex_t sigMutex = PTHREAD_MUTEX_INITIALIZER;


static jmp_buf jmpEnv;


static
void SigHandler(int sig) {

    // This print is not "safe", but ya, I need some feedback when first
    // developing this code.  Note: now the next line is commented out:
    INFO("catch signal %d", sig);

    DASSERT(signalThread);

    if(pthread_equal(pthread_self(), signalThread->pthread))
        longjmp(jmpEnv, sig);
    else
        CHECK(pthread_kill(signalThread->pthread, sig));
}


static void
QueueSignalJobs(int sigNum) {

    // This is the same as getting the job mutex lock like
    // qsjob_lock().
    CHECK(pthread_mutex_lock(&sigMutex));

    CRBNode *i = signalThread->signals.root;
    struct QsSignal *s;

    while(i) {

        s = c_rbnode_entry(i, struct QsSignal, node);
        DASSERT(s);

        if(s->sigNum < sigNum)
            i = i->left;
        else if(s->sigNum > sigNum)
            i = i->right;
        else
            break;
    }
    ASSERT(i, "Entry for signal number %d was not found", sigNum);

    for(uint32_t k = 0; k < s->numSignalJobs; ++k) {
        struct QsSignalJob *sj = *(s->signalJobs + k);
        DASSERT(sj->callback);
        struct QsJob *j = &sj->job;
        DASSERT(j->jobsBlock);
        struct QsThreadPool *tp = j->jobsBlock->threadPool;
        DASSERT(tp);
        // Because we are not running in a thread pool worker thread we
        // must lock the thread pool mutex and check if the thread pool is
        // halted, and not queue the job if it is halted.
        CHECK(pthread_mutex_lock(&tp->mutex));
        if(!tp->halt)
            _qsJob_queueJob(j, false/*have thread pool lock already*/);
        CHECK(pthread_mutex_unlock(&tp->mutex));
    }

    CHECK(pthread_mutex_unlock(&sigMutex));
}


static void
DestroySignalJob(struct QsSignalJob *j) {

    CHECK(pthread_mutex_lock(&sigMutex));

    DASSERT(signalThread);
    DASSERT(j);
    struct QsSignal *s = j->signal;
    DASSERT(s);
    DASSERT(s->numSignalJobs);
    DASSERT(s->signalJobs);

    uint32_t i = 0;
    for(; i < s->numSignalJobs; ++i)
        if(j == *(s->signalJobs + i))
            break;
    DASSERT(j == *(s->signalJobs + i));
    for(++i; i < s->numSignalJobs; ++i)
        *(s->signalJobs + i - 1) = *(s->signalJobs + i);

    --s->numSignalJobs;

    DZMEM(j, sizeof(*j));
    free(j);

    if(s->numSignalJobs) {
#ifdef DEBUG
        *(s->signalJobs + s->numSignalJobs) = 0;
#endif
        s->signalJobs = realloc(s->signalJobs,
                s->numSignalJobs*sizeof(*s->signalJobs));
        ASSERT(s->signalJobs, "realloc(,%zu) failed",
                s->numSignalJobs*sizeof(*s->signalJobs));
    } else {
#ifdef DEBUG
        *s->signalJobs = 0;
#endif
        free(s->signalJobs);
        s->signalJobs = 0;
        c_rbnode_unlink(&s->node);
        DZMEM(s, sizeof(*s));
        free(s);

        if(!signalThread->signals.root) {
            // There are no more signal jobs.  The signalThread object and
            // the thread are no longer needed.
            signalThread->done = 1;
            CHECK(pthread_kill(signalThread->pthread, WAKEUP_SIG));
            CHECK(pthread_join(signalThread->pthread, 0));
            DSPEW("Joined signal thread");
            DZMEM(signalThread, sizeof(*signalThread));
            free(signalThread);
            signalThread = 0;
        }
    }

    CHECK(pthread_mutex_unlock(&sigMutex));
}


static void *
SleepyThread(struct QsSignalThread *signalThread) {

    CHECK(pthread_mutex_lock(&sigMutex));

    DASSERT(signalThread->barrier);

    int ret = pthread_barrier_wait(signalThread->barrier);
    ASSERT(ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD);

    while(true) {

        // We must get a copy of the signal mask before we unlock
        // sigMutex.
        sigset_t sigset;
        memcpy(&sigset, &signalThread->sigset, sizeof(sigset));

        CHECK(pthread_mutex_unlock(&sigMutex));
 
        int sig = setjmp(jmpEnv);

        if(sig && sig != WAKEUP_SIG)
            // Queue signal jobs.
            QueueSignalJobs(sig);

        if(signalThread->done)
            break;

        // pthread_sigmask() does incur a mode switch (a system call); so
        // while this method is robust, it is a little costly.
        //
        ASSERT(0 == pthread_sigmask(SIG_UNBLOCK, &sigset, 0));

        pause();

        // By blocking the signals now we may be able to not miss events
        // from these signals.  We get them just above this line after
        // pthread_sigmask(SIG_UNBLOCK,,).
        //
        ASSERT(0 == pthread_sigmask(SIG_BLOCK, &sigset, 0));

        if(signalThread->done)
            break;

        CHECK(pthread_mutex_lock(&sigMutex));
    }


    CHECK(pthread_mutex_unlock(&sigMutex));

    return 0;
}


static bool Work(struct QsSignalJob *j) {

    DASSERT(j->callback);

    qsJob_unlock(&j->job);

    j->callback(j->userData);

    qsJob_lock(&j->job);

    return false;
}


static void AddSignalJob(struct QsJobsBlock *b, struct QsSignal *s,
        int (*callback)(void *userData), void *userData) {

    s->signalJobs = realloc(s->signalJobs,
            (++s->numSignalJobs)*sizeof(*s->signalJobs));
    ASSERT(s->signalJobs, "realloc(,%zu) failed",
            (s->numSignalJobs)*sizeof(*s->signalJobs));

    DASSERT(s->numSignalJobs);

    struct QsSignalJob *j = calloc(1, sizeof(*j));
    ASSERT(j, "calloc(1,%zu) failed", sizeof(*j));

    *(s->signalJobs + s->numSignalJobs -1) = j;

    qsJob_init(&j->job, b, (bool (*)(struct QsJob *job)) Work,
            (void (*)(struct QsJob *)) DestroySignalJob, 0);
    qsJob_addMutex(&j->job, &sigMutex);
    j->callback = callback;
    j->userData = userData;
    j->signal = s;
}


static
void AddSignal(struct QsJobsBlock *b, int sigNum,
        int (*callback)(void *userData), void *userData) {

    DASSERT(signalThread);
    DASSERT(sigNum);
    DASSERT(callback);

    CRBNode **i = &signalThread->signals.root;
    CRBNode *parent = 0;

    while(*i) {

        parent = *i;

        struct QsSignal *s = c_rbnode_entry(*i,
                struct QsSignal, node);
        if(s->sigNum < sigNum)
            i = &parent->left;
        else if(s->sigNum > sigNum)
            i = &parent->right;
        else {
            // Add another callback to this existing signal number
            // catcher thingy.
            DASSERT(s->numSignalJobs);
            AddSignalJob(b, s, callback, userData);
            return;
        }
    }

    struct QsSignal *s = calloc(1, sizeof(*s));
    ASSERT(s, "calloc(1,%zu) failed", sizeof(*s));
    s->sigNum = sigNum;
    AddSignalJob(b, s, callback, userData);

    ASSERT(0 == sigaddset(&signalThread->sigset, sigNum));
    ASSERT(0 == pthread_sigmask(SIG_BLOCK, &signalThread->sigset, 0));

    { // Add the signal catcher for signal number sigNum
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = SigHandler;
        act.sa_flags = SA_NODEFER;
        CHECK(sigaction(sigNum, &act, 0));
    }
#if 1
    DASSERT(b->block.name);
    DSPEW("Adding signal %d to block \"%s\"",
                sigNum, b->block.name);
#endif
    c_rbtree_add(&signalThread->signals, parent, i, &s->node);
}


struct QsSignalJob *qsSignalJobCreate(int sigNum,
        int (*callback)(void *userData), void *userData) {

    NotWorkerThread();

    ASSERT(sigNum != WAKEUP_SIG);
    ASSERT(sigNum);
    ASSERT(callback);

    // But we need another stink'n mutex lock to protect across different
    // graphs.
    CHECK(pthread_mutex_lock(&apiMutex));

    struct QsJobsBlock *b =
        // Note: we are in a block's declare() or configure()
        // callback so we will already have a graph mutex lock.
        GetBlock(CB_DECLARE|CB_CONFIG, QS_TYPE_JOBS, 0);

    DASSERT(b->block.graph);
    DSPEW("creating signal %d callback for block \"%s\"",
            sigNum, b->block.name);

    CHECK(pthread_mutex_lock(&sigMutex));


    if(!signalThread) {

        pthread_barrier_t barrier;
        signalThread = calloc(1, sizeof(*signalThread));
        ASSERT(signalThread, "calloc(1,%zu) failed", sizeof(*signalThread));

        ASSERT(0 == sigemptyset(&signalThread->sigset));
        ASSERT(0 == sigaddset(&signalThread->sigset, WAKEUP_SIG));

        { // Add the signal catcher for signal number WAKEUP_SIG
          // which we use to stop the SleepyThread() pthread.
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_handler = SigHandler;
            act.sa_flags = SA_NODEFER;
            CHECK(sigaction(WAKEUP_SIG, &act, 0));
        }

        ASSERT(0 == pthread_sigmask(SIG_BLOCK,
                        &signalThread->sigset, 0));

        CHECK(pthread_barrier_init(&barrier, 0, 2));
        signalThread->barrier = &barrier;

        CHECK(pthread_create(&signalThread->pthread, 0,
                    (void *(*) (void *)) SleepyThread,
                    (void *) signalThread));

        AddSignal(b, sigNum, callback, userData);

        CHECK(pthread_mutex_unlock(&sigMutex));

        int ret = pthread_barrier_wait(&barrier);
        ASSERT(ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD);

    } else {

        ASSERT(0 == pthread_sigmask(SIG_BLOCK, &signalThread->sigset, 0));
        AddSignal(b, sigNum, callback, userData);
        CHECK(pthread_mutex_unlock(&sigMutex));
    }

    ASSERT(0 == pthread_sigmask(SIG_UNBLOCK, &signalThread->sigset, 0));

    CHECK(pthread_mutex_unlock(&apiMutex));

    return 0;
}


void qsSignalJobDestroy(struct QsSignalJob *signalJob) {

    qsJob_cleanup(&signalJob->job);
}



