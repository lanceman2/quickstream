
struct QsGraph;
struct QsBlock;


struct QsThreadPool {

    // associated graph for this threadPool
    struct QsGraph *graph;

    // maxThreads does not change at flow/run time, so we need no mutex to
    // access it.
    uint32_t maxThreads;

    // We use "next" to make a list of threadPools in graph.
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

    //
    // Job queue that is waiting for a worker thread starting at "first"
    // and going toward "last".
    //
    // Need mutex to access.
    //
    struct QsSimpleBlock *first, *last;
};



// pop off the first in the queue
//
static inline
struct QsSimpleBlock *PopBlockFromThreadPoolQueue(
        struct QsThreadPool *tp) {

    struct QsSimpleBlock *b = tp->first;
    if(!b) {
        DASSERT(tp->last == 0);
        // There was no blocks in the queue.
        return b; // 0
    }

    DASSERT(b->blockInThreadPoolQueue);
    DASSERT(b->prev == 0);

    tp->first = b->next;

    if(tp->first) {
        b->next = 0;
        DASSERT(tp->first->prev == b);
        tp->first->prev = 0;
    } else {
        tp->last = 0;
    }

    b->blockInThreadPoolQueue = false;

    return b;
}


// Remove the block entry from the thread pool the queue
//
// We need to do this sometimes instead of
// PopBlockFromThreadPoolQueue().
//
static inline
void RemoveBlockFromThreadPoolQueue(struct QsSimpleBlock *b) {

    DASSERT(b->blockInThreadPoolQueue);

    struct QsThreadPool *tp = b->threadPool;

    // Remove block, b, from old threadPool doubly linked list.
    if(b->prev) {

        DASSERT(b != tp->first);
        b->prev->next = b->next;

    } else {

        DASSERT(b == tp->first);
        tp->first = b->next;
    }
    if(b->next) {

        DASSERT(b != tp->last);
        b->next->prev = b->prev;
        b->next = 0;

    } else {

        DASSERT(b == tp->last);
        tp->last = b->prev;
    }
    b->prev = 0;
    
    b->blockInThreadPoolQueue = false;
}


// Add the block to the last entry in the queue
static inline
void AddBlockToThreadPoolQueue(struct QsSimpleBlock *b) {


    DASSERT(b->blockInThreadPoolQueue == false);
    
    DASSERT(b->next == 0);
    DASSERT(b->prev == 0);

    struct QsThreadPool *tp = b->threadPool;

    if(tp->last) {
        DASSERT(tp->first);
        b->prev = tp->last;
        tp->last->next = b;
        tp->last = b;
    } else {
        DASSERT(tp->first == 0);
        tp->first = tp->last = b;
    }

    b->blockInThreadPoolQueue = true;
}
