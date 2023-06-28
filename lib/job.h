
extern
bool StreamWork(struct QsStreamJob *j);


extern
void CheckStreamFinished(struct QsGraph *g);


// A job is the opening to the possibility of a event that can be worked
// on by worker threads in the thread pool.
//
// The jobs are allocated with and in the job blocks.  That is blocks that
// can have jobs.  Not all block types can have jobs.  Job blocks will have a
// number of jobs. Jobs are queued in the job blocks job queue.  Active (busy)
// jobs are being worked on by a worker thread in the thread pool and are
// not queued.
//
// QsJob is a base class.  There are different kinds of jobs.
//
struct QsJob {

    // If set, this is called in qsJob_cleanup() at the end.
    //
    // Destructor for a given type of job.
    //
    void (*destroy)(struct QsJob *j);

    // We point to the job block that owns this job.
    //
    // A job block is a block that has jobs.  We may just call it block
    // sometimes.
    //
    struct QsJobsBlock *jobsBlock;

    // These two pointers, n and p, are for keeping a stack list of all
    // jobs that exist in the owning job block, in QsJobsBlock::jobsStack.
    // The jobs in this list may be queued or not.
    //
    // NOTE: Do not mix "n" with "next", or "p" with "prev" in the code.
    struct QsJob *n, *p; // next and prev in QsJobsBlock::jobsStack

    // Jobs are kept in a doubly listed list that is in the job block
    // queue.  If j->inQueue is set it's in the block queue:
    // QsJobsBlock::first and QsJobsBlock::last.
    //
    struct QsJob *next, *prev; // in block work/event queue

    // The lock() and unlock() only use mutexes with jobs that may have
    // jobs queued in the inner job event loop.

    // The order of array elements is the locking order.  We unlock in
    // reverse order of that order.  We use the mutex address in
    // increasing order.
    //
    // This array has numMutexes + 2 elements, so that it is 0 terminated
    // in both directions; with a 0 in the first element and a 0 in the
    // last element.
    //
    // Array of pointers to mutexes, hence pthread_mutex_t **mutexes
    //
    // A length for this mutexes[] array is not needed to be kept.  We
    // just so happen to need to go through the array anytime we edit it
    // or use it.
    //
    pthread_mutex_t **mutexes; // = [ 0, &m0, &m1, ..., &(mN-1), 0 ]
    //
    // *firstMutex points to the first mutex pointer in mutexes.
    // *lastMutex points to the last mutex pointer in mutexes.
    //
    // firstMutex and lastMutex are redundant, but keep us from having to
    // dereference a pointer at each qsJob_lock() and corresponding
    // qsJob_unlock() calls.
    pthread_mutex_t **firstMutex, **lastMutex;


    // What the worker thread will do.  work() is set by the
    // inheriting object.
    //
    // The work() function must handle queuing of peer jobs and keep
    // going (returning true) until all "would have been queued"
    // possibilities are used up.
    //
    // Example: a parameter setter callback is called until it's internal
    // queue/buffer is empty and there is no more new data to "set".
    bool (*work)(struct QsJob *j);


    // Just one of the next two pointers are set, so they double purpose
    // as flags (being set or not) and pointers (when set).  I couldn't
    // see using a union when there is very little memory gained, if at
    // all with data structure padding.
    //
    // This job may queue jobs listed in "peers" declared below.
    //
    // peers is a 0 terminated array.  It gets realloc()ed each
    // time it changes size.  Note we said *peers and not peers.
    //
    // sharedPeers is a handle to the peers that is shared by all in the
    // list.
    //
    // The order of the elements is kept as they are added.
    //
    // If peers is copied from the peers argument of qsJob_init(),
    // then the peer jobs list will be shared with other jobs, and so the
    // list will include this job too, and any job added to the job peers
    // list will be shared between all the jobs, forming a fully connected
    // web of jobs.  If this job peers list it not shared this job will
    // not be in the list and jobs will not necessarily form a fully
    // connected web of jobs; as is the case for stream jobs.
    //
    // Note: this is a pointer to a pointer to an array of job pointers.
    // We have an extra layer of indirection so that when the list is
    // realloc() (re-allocated) we set the pointer in the 2nd layer of
    // pointers, and the shared array pointer that does not change and is
    // set in the "handle" where we keep an array pointer.  Welcome to
    // pointer indirection hell...  I've never seen a use for *** level
    // pointers until now.  Okay, maybe I've seen it with strings
    // before.
    //
    // sharedPeers pointer that points to a handle that points to an array
    // of job pointers.  The layers of indirection needed so we may
    // realloc() the array without having to change all job peer
    // structures.
    //
    struct QsJob ***sharedPeers; // Ya, fuck'n 3 stars.
    //
    // And either peers is set or sharedPeers is set, and not both.
    //
    // Not shared version of peers:
    struct QsJob **peers;


    // Is the job in the job block's queue?
    //
    // What happens is: jobs can see that the work is needed many times
    // before the work() is called; so we need to stop it from being
    // queued more than once.
    //
    bool inQueue;

    // Is the job being, or about to be, worked on?
    //
    // The job will not be queued in the job block's queue when this
    // "busy" flag is set.  So long as this "busy" flag is set we do not
    // queue this job into the job block queue.  So ya, the work()
    // function must finish all work before returning, even work that
    // would have been queued had it not been "busy".
    //
    // We can also use this "busy" signal to have it not be queued.
    // Maybe "busy" should be named "doNotQueue".
    //
    bool busy;


    //bool keepWorking;
};



///////////////////////////////////////////////////////////////////
//  To create a new job
///////////////////////////////////////////////////////////////////
//
//
//    j = malloc();
//    qsJob_init(j,b,work);
//
//
//
//    .
//    qsJob_addMutex(j,m1);
//    qsJob_addMutex(j,m2);
//    .
//    
//    // thread pool lock here ????
//
//    .
//    qsJob_addPeer(j,pA); // zero or more peers
//    .
//    qsJob_addPeer(j,pB);
//    .
//    .
//
//
//  // The added mutexes need to protect all the added peers too.
//



static inline
void qsJob_lock(struct QsJob *j) {

    // This needs to be fast.
    //
    // TODO: Maybe remove the error checking?
    for(pthread_mutex_t **mutex = j->firstMutex; *mutex; ++mutex)
        CHECK(pthread_mutex_lock(*mutex));
}


static inline
void qsJob_unlock(struct QsJob *j) {

    // This needs to be fast.
    //
    // TODO: Maybe remove the error checking?
    for(pthread_mutex_t **mutex = j->lastMutex; *mutex; --mutex)
        CHECK(pthread_mutex_unlock(*mutex));
}


static inline
void qsJob_work(struct QsJob *j, struct QsWhichJob *wj) {

    qsJob_lock(j);

    // So now this job will not get queued in this job block's job queue;
    // it's "busy".

    // Setting this job, j, to the thread specific data, so we can find
    // the job when the worker thread's block code calls the
    // libquickstream block API without having the block code to know
    // about block objects.
    //
    wj->job = j;

    // The work function may queue other jobs, but not this, j, one.
    //
    // We require that work() optionally call qsJob_unlock(j) before it
    // calls a job's callback function, and to call qsJob_lock(j) after
    // calling a block's callback function, if they wish to let the
    // "work()" call happen in parallel with other job's work() callback
    // functions.
    //
    // If the work() function unlocks the job lock, with qsJob_unlock(),
    // it must call qsJob_lock() before returning.
    //
    // The work() callback function must have the job lock, or other
    // (inter-thread memory access) protection, while calling
    // qsJob_queueJob() for any jobs it queues.
    //
    // Writing a QsJob work() callback requires an understanding of
    // multi-threaded programming; but higher level interface wrapper
    // callback functions can handle the needed thread synchronization,
    // and simplify the user interface for dumb users (block writers).
    //
    while(j->work(j));

    wj->job = 0;

    // If j->work() was written correctly we should the job lock now.
}


// We started this code without low priority stream job queuing, and so we
// need the older code accessible for testing; so without WLPSQ defined
// we test with the old code and with WLPSQ defined we test with the
// new code.  Production/stable code should have WLPSQ defined.
//
// Time will tell if we can remove this pre-compiler option.  We hope to
// remove the #else code and keep the code that comes about with WLPSQ
// defined.
//
#define WLPSQ // WITH LOW PRIORITY STREAM QUEUING


// We just wanted this to remove thinking about the 3 pointer,
// first, last, and firstStream, block job queue thingy.
// The stream jobs have a lower queue priority.
//
// Add this job, j, in the block's job queue.
// If there are stream jobs in the queue, then put it
// just before the stream job if it's not a stream job, or
// last if it is a stream job.
//
static inline void
ReallyQueueJob(struct QsJobsBlock *b, struct QsJob *j) {

    DASSERT(j);
    DASSERT(b);
    DASSERT(!j->busy);
    DASSERT(!j->inQueue);
    DASSERT(!j->next);
    DASSERT(!j->prev);

#ifdef WLPSQ
    ///////////////////////////////////////////////////////
    //   WITH LOW PRIORITY STREAM QUEUING (WLPSQ)
    ///////////////////////////////////////////////////////

    if(j->work == (void *) StreamWork || !b->firstStream) {
        // Queuing a stream job, j, or a regular (non-stream) job without
        // stream jobs present.
        //
        // Put j at block's, b->last, in the block's job queue list.
        j->prev = b->last;
        if(b->last) {
            DASSERT(!b->last->next);
            DASSERT(b->first);
            b->last->next = j;
        } else {
            DASSERT(!b->first);
            b->first = j;
        }
        b->last = j;

        if(!b->firstStream && j->work == (void *) StreamWork)
            // It's the first and only stream job.
            b->firstStream = j;

    } else {
        // Queuing a non-stream job, j, and there are stream jobs
        // present.
        //
        // Put j before b->firstStream in the block's job queue list.
        DASSERT(b->first);
        DASSERT(b->last);
        // Kind of dumb, but remember this is true:
        DASSERT(b->firstStream);
        DASSERT(b->firstStream->work == (void *) StreamWork);

        j->next = b->firstStream;
        j->prev = b->firstStream->prev;

        if(b->firstStream->prev) {
            DASSERT(b->firstStream != b->first);
            // The job before the first stream job must not be a stream
            // job.
            DASSERT(b->firstStream->prev->work != (void *) StreamWork);
            b->firstStream->prev->next = j;
        } else {
            DASSERT(b->firstStream == b->first);
            b->first = j;
        }
        b->firstStream->prev = j;
    }

#else
    ///////////////////////////////////////////////////////
    //   WITHOUT LOW PRIORITY STREAM QUEUING
    ///////////////////////////////////////////////////////


    // put j in block's, b->last, in the block's job queue.
    DASSERT(!j->next);
    j->next = 0;
    j->prev = b->last;
    if(b->last) {
        DASSERT(!b->last->next);
        DASSERT(b->first);
        b->last->next = j;
    } else {
        DASSERT(!b->first);
        b->first = j;
    }
    b->last = j;

#endif

    j->inQueue = true;
}


// We just wanted this to remove thinking about the 3 pointer,
// first, last, and firstStream, block job queue thingy.
//
// Remove this job, j, from anywhere in the block's job queue.
//
static inline void
ReallyDequeueJob(struct QsJobsBlock *b, struct QsJob *j) {

    DASSERT(j);
    DASSERT(b);
    DASSERT(!j->busy);
    DASSERT(j->inQueue);


#ifdef WLPSQ
    ///////////////////////////////////////////////////////
    //   WITH LOW PRIORITY STREAM QUEUING
    ///////////////////////////////////////////////////////

    if(j->work == (void *) StreamWork && j == b->firstStream)
        b->firstStream = j->next;

#endif


    if(j->next) {
        DASSERT(j != b->last);
        j->next->prev = j->prev;
    } else {
        DASSERT(j == b->last);
        b->last = j->prev;
    }

    // It's more likely we are dequeuing the first so
    // we do !j->prev in the first conditional.
    //
    if(!j->prev) {
        DASSERT(j == b->first);
        b->first = j->next;
    } else {
        DASSERT(j != b->first);
        j->prev->next = j->next;
        j->prev = 0;
    }
    j->next = 0;


    j->inQueue = false;
}

// Give a hoot don't pollute (the macro namespace).
#ifdef WLPSQ
#  undef WLPSQ
#endif



// We call this with a thread pool halt in qsGraph_stop().
//
static inline void
CheckDequeueJob(struct QsJobsBlock *b, struct QsJob *j) {

    DASSERT(j);
    // Just making sure that we are using this just for
    // stream jobs.
    DASSERT(j->work == (void *) StreamWork);
    DASSERT(b);
    DASSERT(b == j->jobsBlock);
    DASSERT(!j->busy);
    struct QsThreadPool *tp = b->threadPool;
    DASSERT(tp);
    DASSERT(tp->halt);
    DASSERT(!tp->numWorkingThreads);
    DASSERT(!b->busy);
    DASSERT(!j->busy);
 

    if(!j->inQueue) {
        DASSERT(!j->next);
        DASSERT(!j->prev);
        return;
    }


    ReallyDequeueJob(b, j);


    if(b->inQueue && !b->first) {

        // j was the last job in the jobs block queue so we must remove
        // the block from the thread pool block queue.
        DASSERT(!b->last);
        
        if(b->next) {
            DASSERT(b != tp->last);
            b->next->prev = b->prev;
        } else {
            DASSERT(b == tp->last);
            tp->last = b->prev;
        }
        if(b->prev) {
            DASSERT(b != tp->first);
            b->prev->next = b->next;
            b->prev = 0;
        } else {
            DASSERT(b == tp->first);
            tp->first = b->next;
        }
        b->next = 0;
        b->inQueue = false;
    }
}


// We must have a thread pool mutex lock before calling this, or a thread
// pool halt lock.
//
// Put a block back in the thread pool block queue in the "last" position.
//
// There must be jobs in the block's job queue to call this.
//
static inline void QueueBlock(struct QsThreadPool *tp,
        struct QsJobsBlock *b) {

    // There must be jobs in the block's job queue:
    DASSERT(b->first);
    DASSERT(b->first->inQueue);
    DASSERT(b->last);
    DASSERT(b->last->inQueue);

    DASSERT(!b->next);
    DASSERT(!b->prev);

    if(tp->last) {
        DASSERT(tp->first);
        DASSERT(!tp->last->next);
        DASSERT(!tp->first->prev);
        tp->last->next = b;
    } else {
        DASSERT(!tp->first);
        tp->first = b;
    }
    b->prev = tp->last;
    tp->last = b;

    DASSERT(!b->inQueue);
    b->inQueue = true;
}


// We must have a thread pool mutex lock before calling this.
//
// Put a job block back in the thread pool block queue in the "first"
// position.
//
// There must be jobs in the job block's job queue to call this.
//
static inline void ReQueueBlock(struct QsThreadPool *tp,
        struct QsJobsBlock *b) {

    // There must be jobs in the block's job queue:
    DASSERT(b->first);
    DASSERT(b->first->inQueue);
    DASSERT(b->last);
    DASSERT(b->last->inQueue);

    DASSERT(!b->next);
    DASSERT(!b->prev);

    if(tp->first) {
        DASSERT(!tp->first->prev);
        DASSERT(tp->last);
        tp->first->prev = b;
    } else {
        DASSERT(!tp->last);
        tp->last = b;
    }
    b->next = tp->first;
    tp->first = b;

    DASSERT(!b->inQueue);
    b->inQueue = true;
}


// We need a thread pool mutex lock to call this:
//
static inline
void CheckLaunchWorkers(struct QsThreadPool *tp) {

    if(!tp->first || tp->numWorkingThreads >= tp->maxThreadsRun
            || tp->halt || tp->signalingOne)
        // We do not have a block in the thread pool queue, or we have the
        // maximum number of worker threads (or more) now working, or we
        // are halting this thread pool OR we are launching a worker
        // thread now, or we are trying to join worker threads now, or we
        // are in the process of signaling the tp->cond to wake a worker
        // thread.  Enough crap?
        return;


    // We'll take more working threads please:

    if(tp->numWorkingThreads < tp->numThreads) {
        // Wake up a worker thread.
        tp->signalingOne = true;
        CHECK(pthread_cond_signal(&tp->cond));
    } else if(tp->numThreads < tp->maxThreadsRun)
        // Make a new worker thread.
        _LaunchWorker(tp);
}


// We must have the job lock before calling this.
//
// mutex locking order of:
//
//    1. job lock before calling this, then
//    2. thread pool lock in this function; if ...
//
// Unlock in reverse of that:
//
//    1. unlock thread pool in this function if ..., then
//    2. unlock job after calling this function
//
//
static inline
void _qsJob_queueJob(struct QsJob *j, bool getTPL) {

    DASSERT(j);

    struct QsJobsBlock *b = j->jobsBlock;
    DASSERT(b);
    struct QsThreadPool *tp = b->threadPool;
    DASSERT(tp);

    if(getTPL)
        CHECK(pthread_mutex_lock(&tp->mutex));

    DASSERT(tp->numThreads);


    if(j->busy || j->inQueue) {

        // No need to queue the job, it's working on it now or
        // it's already queued.
        if(getTPL)
            CHECK(pthread_mutex_unlock(&tp->mutex));
        return;
    }

    // put j in block's, b->last, in the block's job queue.
    ReallyQueueJob(b, j);


    if(j->work == (void *) StreamWork)
        ++tp->graph->streamJobCount;


    if(b->busy || b->inQueue) {
        // The block is in the thread pool queue or is being acted on
        // now.
        if(getTPL)
            CHECK(pthread_mutex_unlock(&tp->mutex));
        return;
    }

    // Queue the block, b, into the thread pool's block queue as the last
    // block:
    QueueBlock(tp, b);


    // TODO: Why was this if() here????
    //if(!tp->numWorkingThreads)
    CheckLaunchWorkers(tp);

    if(getTPL)
        CHECK(pthread_mutex_unlock(&tp->mutex));
}


static inline
void qsJob_queueJob(struct QsJob *j) {
    _qsJob_queueJob(j, true/*get thread pool mutex lock*/);
}


// We must have the thread pool mutex lock before calling this, or a
// thread pool halt lock.
//
// This will not unlock the thread pool mutex.
//
// This will get the block even if the thread pool is "halted";
// that is if it is in the thread pool block queue.
//
static inline
bool PullBlock(struct QsThreadPool *tp, struct QsJobsBlock *b) {

    DASSERT(b->threadPool == tp);

    if(!b->inQueue) {
        DASSERT(tp->first != b);
        DASSERT(tp->last != b);
        // We will not DASSERT() the whole list.
        return false; // don't have one to pull
    }


    if(b->next) {
        DASSERT(tp->last != b);
        b->next->prev = b->prev;
    } else {
        DASSERT(tp->last == b);
        tp->last = b->prev;
    }

    if(b->prev) {
        DASSERT(tp->first != b);
        b->prev->next = b->next;
        b->prev = 0;
    } else {
        DASSERT(tp->first == b);
        tp->first = b->next;
    }
    b->next = 0;

    // There should be jobs in the block's job queue.
    DASSERT(b->first);
    DASSERT(b->last);

    b->inQueue = false;

    return true; // got one.
}


// We must have the thread pool mutex lock before calling this.
//
// This will not unlock the thread pool mutex.
//
// This will not get the block if the thread pool is "halted".
//
static inline
struct QsJobsBlock *PopBlock(struct QsThreadPool *tp) {

    DASSERT(tp);

    if(!tp->first) {
        DASSERT(!tp->last);
        return 0;
    }

    if(tp->halt || tp->numThreads > tp->maxThreadsRun)
        // We have a block with a job ready for work but we are halting
        // this thread pool, tp; so we just don't give out the list of
        // jobs in the block, like below this block of code;
        //
        // OR we have had a change in the number or worker threads.
        return 0;

    DASSERT(tp->last);

    // Pop the next block, b:
    struct QsJobsBlock *b = tp->first;
    DASSERT(b->threadPool == tp);
    //
    DASSERT(!b->prev);
    tp->first = b->next;
    if(b->next) {
        DASSERT(tp->last != b);
        DASSERT(b->next->prev == b);
        b->next->prev = 0;
        b->next = 0;
    } else {
        DASSERT(tp->last == b);
        tp->last = 0;
    }
    DASSERT(b->inQueue);
    b->inQueue = false;

    // There should be jobs in the block's job queue.
    DASSERT(b->first);
    DASSERT(b->last);

    return b;
}


// We must have the thread pool mutex lock before calling this.
//
// This will not unlock the thread pool mutex.
//
static inline
struct QsJob *PopJob(struct QsJobsBlock *b) {

    DASSERT(b);

    if(!b->first) {
        DASSERT(!b->last);
        return 0;
    }

    struct QsThreadPool *tp = b->threadPool;
    DASSERT(tp);

#if 1
    if(tp->halt || tp->numThreads > tp->maxThreadsRun) {
        // Re-queue the block so it's jobs may be able to be used after
        // the thread pool halt;
        //   OR
        // we have had a change in the number or worker threads.
        //
        ReQueueBlock(b->threadPool, b);
        // Do not return any more jobs for this halt.
        return 0;
    }
#endif

    // Pop the next job, j:
    //
    struct QsJob *j = b->first;
    //
    DASSERT(!j->prev);
    ReallyDequeueJob(b, j);


    DASSERT(j->jobsBlock == b);

    return j;
}



extern
void qsJob_init(struct QsJob *j, struct QsJobsBlock *b,
        bool (*work)(struct QsJob *job),
        void (*destroy)(struct QsJob *j),
        struct QsJob ***peers);

extern
void qsJob_addPeer(struct QsJob *jA, struct QsJob *jB);


extern
void qsJob_removePeer(struct QsJob *jA, struct QsJob *jB);


extern
void qsJob_cleanup(struct QsJob *j);

extern
void qsJob_addMutex(struct QsJob *j, pthread_mutex_t *mutex);

extern
void qsJob_removeMutex(struct QsJob *j, pthread_mutex_t *mutex);
