
struct QsGraph;
struct QsBlock;
struct QsThreadPool;


enum QsGraphFlowState {

    // This state is just to make sure the Graph state always goes from
    // paused -> ready -> flowing -> paused -> ready -> flowing -> ...
    //
    // Yes we could look at the data in the QsGraph, but that may not be
    // consistent if there is a failure in one of the steps.

    // blocks are still being created and connected
    // The stream has been stopped or was never flowing.
    QsGraphPaused = 0,
    // start() callbacks have been called and stream buffers allocated
    // and superBlocks have been flattened (if that means anything).
    QsGraphReady,
    // flowing or flushing
    QsGraphFlowing,
    // If we can no longer run this graph because a callback returned
    // error less than 0.
    QsGraphFailed
};



// A singly linked list of all graphs.  We do not expect a lot of them.
extern struct QsGraph *graphs;
extern pthread_t mainThread;

// Used to get/set thread specific pointer to block structs
// in block user functions.
extern pthread_key_t _qsGraphKey;


// Get the current block while in a block callback.
static inline
struct QsBlock *GetBlock(void) {
    struct QsBlock *b = pthread_getspecific(_qsGraphKey);
    ASSERT(b, "pthread_getspecific() failed to get block");
    DASSERT(b->graph);
    DASSERT(b->name);
    return b;
}


struct QsGraph {

    // List of blocks.  Indexed by name.  This gives us fast lookup by
    // block name.
    struct QsDictionary *blocks;

    // Another list of the blocks, as a doubly linked list of all the
    // blocks in the order in which they are loaded.  So we may call block
    // callback functions in load order and reverse load order.  This list
    // contains both simple blocks and super blocks.
    //
    // Note: super blocks do not have flow() and flush() functions.
    //
    struct QsBlock *firstBlock, *lastBlock;

    // For the singly linked list of graphs.
    struct QsGraph *next;

    // Just the main thread should access this flag.
    enum QsGraphFlowState flowState;


    // We can have any number of thread pools with any amount of threads
    // in each thread pool.  Every block in this graph must be assigned to one
    // thread pool.
    //
    // A special case we needed was a heavy block needing a thread pool
    // with CPU thread affinity set.  That's the case that precipitated
    // having more than one thread pool.  Sometimes setting CPU thread
    // affinity can have a big performance effect.
    //
    // A singly linked list of thread pools.  "threadPools" points to the
    // last thread pool created.
    //
    struct QsThreadPool *threadPools;

    // The main (master) thread waits on this conditional in qsGraphWait()
    // while the worker threads run the stream flow.
    //
    // These 3 variables are not used if the main thread runs the graph
    // without any worker threads.
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool masterWaiting;

    // numWorkingThreads is the number of launched worker threads running
    // in all thread pools in this graph be they idle or not.  Need graph
    // mutex to access.
    uint32_t numWorkingThreads;
    // numIdleThreads is the number of paused/idle/waiting worker threads
    // in all thread pools.  Need graph mutex to access.
    uint32_t numIdleThreads;


    // This list of filter block connections is not used while the stream
    // is running (flowing).  It's queried at stream start, and the
    // QsSimpleBlock data structs are setup at startup.  The QsSimpleBlock
    // data structures are used when the stream is running.
    //
    // qsBlockConnect() is not called at flow time.
    //
    // tallied filter block connections from qsBlockConnect():
    struct QsConnection {

        struct QsBlock *from; // from filter block
        struct QsBlock *to;   // to filter block

        uint32_t fromPortNum; // output port number for from filter block
        uint32_t toPortNum;   // input port number for to filter block

    } *connections; // malloc()ed array of connections

    uint32_t numConnections;// length of connections array
};
