

struct QsGraph;
struct QsBlock;



struct QsThreadPool {

    // associated graph for this threadPool
    struct QsGraph *graph;

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
    //   2. working: calling work() on by a running thread
    //   3. waiting: in an un-runnable work state due to
    //        3a) waiting on stream buffer or parameter input/output
    //          calling pthread_cond_wait().
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


enum QsGraphFlowState {

    // This state is just to make sure the Graph state always goes from
    // paused -> ready -> flowing -> paused -> ready -> flowing -> ...
    //
    // Yes we could look at the data in the QsGraph, but that may not be
    // consistent if there is a failure in one of the steps.

    // blocks are still being created and connected
    // The stream has been stopped or was never flowing.
    GraphPaused = 0,
    // start() callbacks have been called and stream buffers allocated
    // and superBlocks have been flattened
    GraphReady,
    // flowing or flushing
    GraphFlowing
};


// We keep a list of 
struct QsBootstrapCallbacks;


// A singly linked list of all graphs.  We do not expect a lot of them.
extern struct QsGraph *graphs;
extern pthread_t mainThread;

struct QsGraph {

    // List of blocks.  Indexed by name.
    struct QsDictionary *blocks;

    // For the singly linked list of graphs.
    struct QsGraph *next;

    // We keep a list of bootstrap callbacks which get called once
    // for each block that exists in this graph.  This lets blocks
    // make some changes to other blocks at block bootstrap time.
    struct QsBootstrapCallbacks *bootstrapCallbacks;


    // Blocks that have at least one output and no inputs are sources.
    //
    // The sources is set when the stream is ready or flowing.
    //
    uint32_t numSources;
    struct QsBlock *sources;


    // Just the main thread should access this flag.
    enum QsGraphFlowState flowState;


    // We can define and set different flow() functions that run the flow
    // graph different ways.  This function gets set in qsGraphFlow().
    //
    uint32_t (*flow)(struct QsGraph *graph);


    // We can have any number of thread pools with any amount of threads
    // in each thread pool.  Every block in this graph must be assigned to one
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

    // The main (master) thread waits on this conditional while the worker
    // threads run the stream flow.
    pthread_cond_t masterCond;
    bool masterWaiting;

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
