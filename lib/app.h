

struct QsApp;
struct QsBlock;


struct QsThreadPool {

    // associated app for this threadPool
    struct QsApp *app;

    // maxThreads does not change at flow/run time, so we need no mutex to
    // access it.
    uint32_t maxThreads;

    pthread_mutex_t mutex;
    // cond is paired with mutex.
    pthread_cond_t cond;  // idle threads just wait with this cond.

    // numThreads is the number of pthreads that exist from this
    // threadPool, be they idle or in the process of calling work().  We
    // must have a ThreadPool mutex lock to access numThreads.
    //
    // numThreads <= maxThreads
    uint32_t numThreads;

    // We do not need to keep list of idle threads.  We just have all idle
    // threads call pthread_cond_wait() with the above mutex and cond.
    //
    // numThreads - numIdleThreads = "number of threads working"
    //
    // Need mutex to access.
    //
    uint32_t numIdleThreads;

    // The blocks may be:
    //
    //   1. queued: waiting in the worker thread queue
    //   2. working: being worked on by a running thread
    //   3. waiting: in an un-runnable work state due to
    //        3a) waiting on stream buffer input/output
    //        3b) blocking read or write file descriptor poll
    //
    // This job queue is for case 1 above.  The other cases 2 and 3 have
    // inherit methods to switch the block to the other states.
    //
    // Job queue that is waiting for a worker thread starting at "first"
    // and going to "last".
    //
    // Need mutex to access.
    //
    struct QsBlock *first, *last;
};



struct QsApp {

    // List of blocks.
    struct QsDictionary *blocks;


    // We can define and set different flow() functions that run the flow
    // graph different ways.  This function gets set in (TODO)
    // qsAppReady()???
    //
    uint32_t (*flow)(struct QsApp *app);


    // We can have any number of thread pools with any amount of threads
    // in each thread pool.  Every block in this app must be assigned to one
    // thread pool.
    //
    // A special case we needed was a heavy block needing a thread pool
    // with CPU thread affinity set.  That's the case that precipitated
    // having more than one thread pool.  Sometimes setting CPU thread
    // affinity can have a big performance effect.
    //
    // realloc() allocated array of threadPools:
    //
    uint32_t numThreadPools;
    struct QsThreadPool *threadPools;
};
