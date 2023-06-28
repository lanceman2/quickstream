
struct QsThreadPool {

    // Almost every variable in this struct is protected by this mutex.
    // name is protected by QsGraph::mutex, graph->mutex.  All other
    // variables in the structure are inter-thread access protected by
    // "mutex", below.
    //
    pthread_mutex_t mutex;
    // For worker threads to wait for work/jobs.
    pthread_cond_t cond;

    // To halt the thread pool.
    //
    // We get the memory for it on the stack.
    //
    // It's also used as a bool flag with values being set or 0.
    //
    pthread_cond_t *haltCond;

    // The worker thread main loop, in WorkerLoop(), will signal this
    // condition variable after waking from pthread_cond_wait(cond,mutex)
    // so that the thread that did the waking can continue.
    //
    // Only one thread can wait at a time on this condition variable
    // by using the signalingOne flag.
    //
    // Protected by thread pool mutex.
    //
    pthread_cond_t wakerCond;


    // Fixed at create and never changes.
    struct QsGraph *graph;

    // Every thread pool gets a unique non-zero name.
    //
    // name is a malloc() (family of functions) allocated string.
    //
    // The graph mutex protects this pointer.
    char *name;

    // We keep a simple stack of thread pools in the graph in
    // QsGraph::threadPoolStack; for finding the default thread pool
    // after a thread pool is destroyed.  We use the graph mutex to
    // control access to next and prev.
    //
    struct QsThreadPool *next, *prev;


    // All the variable below require a mutex lock to access after the
    // structure is initialized in qsGraph_createThreadPool().


    // The maximum number of worker threads that this thread pool can run
    // at one time.  The number of threads can change as the program runs.
    //
    // If there are fewer blocks assigned to this thread pool than
    // maxThreads, then the number of worker thread will be less than
    // numJobsBlocks, the number of blocks assigned to the thread pool.
    //
    uint32_t maxThreads;


    // This is used to increment up the number of threads one at a time,
    // and down one at a time.
    //
    // We control how the worker threads create and exit just one at a
    // time.  Having them all try to create or exit all at once would
    // make a very contentious state.  The threads need to access shared
    // counters in order to be created or exit.  Making them create or
    // exit one at one time makes it so there is less data in this data
    // structure; i.e. no stinking barrier and the like; this is much
    // simpler code too.  Using a barriers (and a mutex) is very heavy
    // handed way of serializing many threads that need to access
    // inter-thread shared memory; and the result is that the threads
    // still run a section code one thread at a time with a barrier or
    // not.
    //
    uint32_t maxThreadsRun;

    // The number of blocks that are assigned to this thread pool.  Thread
    // pool to block assignment can change, so numJobsBlocks will change
    // with that.
    //
    // numJobsBlocks is used to limit the number of worker threads that
    // can run, given that blocks can only have one worker thread run a
    // block callback at a given time.  i.e. numThreads (the number of
    // running threads) should stay less than or equal to numJobsBlocks.
    uint32_t numJobsBlocks;

    // The number of worker threads that are currently running, including
    // sleeping threads that are calling pthread_cond_wait() the work
    // loop.
    uint32_t numThreads;

    // The number of threads that are actively working, and not including
    // threads that are calling pthread_cond_wait().
    //
    // numThreads - numWorkingThreads == number of threads waiting
    //
    // Worker threads can wait in different places in the code, not just
    // in pthread_cond_wait() in file threadPool.c function WorkerLoop().
    // Grepping (grep) for "numWorkingThreads" will find those blocking
    // function calls that make the worker threads "wait".
    //
    uint32_t numWorkingThreads;

    // This is set for the worker threads that is going to be joined.
    pthread_t exitingPthread;


    // All the blocks that have jobs waiting to be worked on:
    //
    // "first" and "last" make a queue in the thread pool.
    //
    // This queue uses QsJobsBlock::prev and QsJobsBlock::next to make a
    // doubly listed list.
    //
    struct QsJobsBlock *first, *last;



    // "halt" is true there is a request to halt the thread pool or the
    // thread pool is halted.
    //
    bool halt;

    // BUG fix for bug in pthread_cond_signal().  Flag so that we only let
    // one worker thread wake from the pthread_cond_wait() on the thread
    // pool condition variable: QsThreadPool::cond.
    bool signalingOne;
};


static inline
uint32_t Get_maxThreadsRun(struct QsThreadPool *tp) {

    uint32_t maxThreadsRun = tp->numJobsBlocks;
    if(maxThreadsRun > tp->maxThreads)
        maxThreadsRun = tp->maxThreads;
    if(!maxThreadsRun)
        maxThreadsRun = 1;
    return maxThreadsRun;
}



extern
struct QsThreadPool *_qsGraph_createThreadPool(struct QsGraph *g,
        uint32_t maxThreads, const char *name);


extern
void LaunchWorker(struct QsThreadPool *tp);
extern
void _LaunchWorker(struct QsThreadPool *tp);



struct QsJob;

// Thread pool halt functions.
void qsJob_threadPoolHaltLockAsync(struct QsJob *j,
        void (*callback)(struct QsJob *j));
extern
void qsJob_threadPoolHaltLock(struct QsJob *j);
extern
void qsJob_threadPoolHaltUnlock(struct QsJob *j);

extern
void qsGraph_threadPoolHaltLock(struct QsGraph *g,
        struct QsThreadPool *tp);
extern
void qsGraph_threadPoolHaltUnlock(struct QsGraph *g);


extern
void _qsThreadPool_destroy(struct QsThreadPool *tp);


extern
void JoinThreads(struct QsThreadPool *tp, uint32_t numThreadsRun);
