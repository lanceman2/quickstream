
struct QsGraph;
struct QsBlock;


struct QsThreadPool {

    // associated graph for this threadPool
    struct QsGraph *graph;

    // maxThreads does not change at flow/run time, so we need no mutex to
    // access it.
    uint32_t maxThreads;

    struct QsThreadPool *next;

    pthread_mutex_t mutex;
    // cond is paired with mutex.
    pthread_cond_t cond;  // idle threads just wait with this cond.

    // numThreads is the number of pthreads that exist from this
    // threadPool, be they idle or in the process of calling flow().  We
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
    //   2. working: calling flow() or other trigger action on by a running
    //      thread
    //   3. waiting: in an un-runnable work state due to
    //        3a) waiting on stream buffer or parameter input/output
    //          calling pthread_cond_wait().
    //        3b) blocking read or write file descriptor poll
    //
    // This job queue is for case 1 above.  The other cases 2 and 3 have
    // inherit methods to switch the block to the other states.
    //
    // Job queue that is waiting for a worker thread starting at "first"
    // and going toward "last".
    //
    // Need mutex to access.
    //
    struct QsSimpleBlock *first, *last;
};
