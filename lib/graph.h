

// Protecting read and write access of memory between concurrent threads
// is an interesting topic.
//
// 1. In the case of "steady state" running, which is thread pools running
// lots of threads and processing lots of events/jobs, we have per thread
// pool mutexes in combination with flag variables to read and write
// event/job queues as the worker threads run, and/or wait for events.
//
// 2. In the case of "transits" like changing the graph/block/job topology
// we use a graph wide recursive mutex and thread pool halt locks, which
// are also recursive.  This graph mutex is not accessed by thread pool
// worker threads.  For example:
//
// - moving a block between thread pools requires first a graph mutex lock
//   and then a thread pool lock of all thread pools in the graph; but for
//
// - creating a new block we only require a graph mutex lock and a thread
//   pool mutex lock for the thread pool that the block is to be
//   affiliated with.  It's only when the new block connects its new
//   event/jobs to existing events/jobs, does it get/need thread pool
//   halt locks.
//
// Since both the graph mutex and the thread pool halt lock are recursive,
// we can nest together many graph/block/job topology changes with just
// "in effect" one graph mutex lock and one thread pool halt lock, though
// there will be many lock calls in succession; and the unlock only
// effectively happens in the "last" unlocking call in the series; where
// for each lock there will be a corresponding unlock.  The recursive
// thread pool halt locks make quickstream graph edit on the fly possible.
// Super blocks can load an arbitrary level and number of other super
// blocks using this lock recursion (as apposed to using spaghetti
// code?).
//
// It should be noted that: the graph mutex is locked at the start of the
// thread pool halt lock.  There are cases to use the graph mutex without
// a thread pool halt lock, but not vise versa is not possible.
// For example the "name" of blocks, graph, and thread pools is only
// protected by the graph mutex.
//
// If you build a graph that has sub-graphs that do not connect
// events/jobs between each other than if they run in separate thread
// pools there may be less overall inter-thread contention, and relatively
// better performance.
//
// At "steady state" running the graph mutex and the thread pool halt
// locks are not used at all.  That is the nature of the thread pool
// halt lock.  A sub-graph can run at "steady state" while another
// sub-graph is in a "transit" state.
//
//
// There is another "external" level of halt lock that is seen by the
// libquickstream.so user that is built on top of the internal "thread
// pool halt" lock.  This external "graph halt" is not recursive and acts
// to halt all existing thread pools and future thread pools in the
// graph.
//

enum Feedback {

    QsStart,
    QsStop,
    QsHalt,
    QsUnhalt
};



struct QsGraph {

    // The QsGraph::parentBlock::parent is 0; all other blocks in the
    // graph's block tree have a parent.
    //
    // "inherit" means do not place data above this in this struct or else
    // code will brake and most likely seg-fault when running.  I grant
    // you inherit could have a different meaning than that.  One could
    // argue that including any data structure/type in any position is
    // inheriting, but that's not what we mean here.  We use this idea of
    // inherit (inherit = top of the structure) in all of the quickstream
    // code.
    //
    struct QsParentBlock parentBlock; // inherit QsParentBlock

    // This is a recursive mutex.
    //
    // The semi-static parts of the graph that can change slowly tend to
    // be protected by this mutex.  In contrast, the data structures that
    // change quickly like the parts of the event/job queues are protected
    // by the thread pool mutexes, a combination of flags in the data
    // structures, and thread pool halt locks (ya, something we made up
    // from pthread primitives).
    //
    pthread_mutex_t mutex;


    // If qsGraph_enter() is called stuff related to this will need to be
    // cleaned up when the graph is destroyed.
    //
    // It turns out the pthreads do not have a null value, so we cannot
    // use the pthread_t as a flag to know if the thread has
    // launched yet.  Just adds a byte plus padding.
    bool haveGraphRunner;
    pthread_t graphRunnerThread;


    // The command queue (cq).
    //
    // cqMutex is a recursive mutex.
    //
    pthread_mutex_t cqMutex;
    pthread_cond_t cqCond;
    //
    // The command queue.  Protected by cqMutex.
    //
    struct QsCommand *firstCommand, *lastCommand;
    //
    // Waiting flag is set when before waiting and unset after waiting.
    bool waiting;
    // A flag that is used for asynchronous destroying of the graph
    // from a block callback.
    bool destroyingGraph;
    //
    //
    // When the stream is running we keep a count of the number of
    // simple blocks that are not finished yet (j->isFinished not set).
    //
    // streamBlockCount is protected by cqMutex.
    //
    uint32_t streamBlockCount;


    // The number of stream jobs queued while the graph stream is running.
    // If it goes to zero the stream will not queue any more jobs for a
    // given stream run.  We call it running to completion.  Not all
    // stream runs run to completion.  They can also run until a user
    // stops them via qsGraph_stop().
    //
    // Note this is an atomic variable so we don't need a mutex to access
    // it, assuming you don't do a double take.
    //
    atomic_uint streamJobCount;


    // As we define things, the graph can be running the streams in it
    // or not.  It's a dream of mine to talk intelligently with someone
    // that understands concurrent processing (multi-thread or
    // multi-process) about wither or not having sub-streams in a graph
    // would be useful.  For now we just have all stream flows in the
    // graph either enabled or not enabled.  You can run more than one
    // graph in a process, so having a single process with some streams
    // on and some streams off is possible, if we have more than one
    // graph in the process.
    bool runningStreams;
    //
    // The number of connected stream inputs for a given stream run.
    uint32_t numInputs;


    // List of thread pools:
    struct QsDictionary *threadPools;

    struct QsDictionary *blocks;
    //uint32_t numBlocks;

    // A list of memory allocations used by blocks to share data between
    // them.  Not used in most blocks.
    struct QsDictionary *memDict;


    // Data stored in the graph for a particular application.  This
    // data gets serialized and stored as a base64 encoded string in
    // the super block when the graph is saved as a super block.
    //
    // So the app uses a string key to get a binary value and casts it to
    // any kind of struct.  One particular key is "quickstreamGUI" which
    // stores block x,y position geometry and other things for this GUI
    // program.  The libquickstream.so API has no notion of block x,y
    // position geometry, hence it's called meta data.
    //
    // Caution is needed as to not bloat the super blocks memory usage, by
    // having large meta data structures.
    //
    struct QsDictionary *metaData;

    // A stack (ordered) of all thread pools that are in this graph; so we
    // may select the last thread pool created as the default thread
    // pool.  Note: this stack has elements that are doubly linked so that
    // we can remove thread pool elements without searching.
    //
    // The top of the stack is the current default thread pool.
    struct QsThreadPool *threadPoolStack;

    // Number of thread pool in that list:
    uint32_t numThreadPools;

    // Internal recursive thread pools halt count.
    uint32_t haltCount;

    // isHalted is the bool that answers the question: Do we have an graph
    // wide thread pool halt from the user?  The External non-recursive
    // graph wide halt is built on top of the internal recursive graph
    // thread pool halt.
    bool isHalted;


    // We needed a hook for quickstreamGUI to hear about graph events.
    // Very minimalistic.  The quickstreamGUI code reaches in and sets
    // this function pointer and fbData the user data for this callback
    // function.  The event types are enumerated.
    void (*feedback)(struct QsGraph *g, enum Feedback fb, void *fbData);
    void *fbData;


    // name of the graph. 
    //
    // points to the top block name.
    char *name;


    // Port dictionaries for when we make port aliases that
    // will go into a saved super block, if we save the graph as a super
    // block DSO (dynamic shared object) module.
    struct QsPortDicts ports;

    // If the qsGraph_create() flag QS_GRAPH_IS_MASTER is set we get the
    // graph mutex lock just after creating the graph.
    //
    // Or when the user calls qsGraph_lock().
    //
    // The graph mutex is a recursive mutex that is also used many many
    // times in many functions that call each other.  So in addition the
    // user can use it with qsGraph_create(), qsGraph_lock(), and
    // qsGraph_unlock(); and this haveGraphLock flag is for that user
    // case.
    //
    // The slow down cost of using a atomic_uint is okay, given this
    // is a transit thing (not very often).
    //
    // If the thread that creates the graph is this "master" it
    // must release the lock, qsGraph_unlock(), in order to let a
    // different thread run graph editing commands.
    atomic_uint haveGraphLock;


    // If this, saveAttributes, is set we save the last state of the
    // config() calls for all the direct children of the graph top parent
    // block.  We use this saved data to save the graph as a super block
    // via qsGraph_saveSuperBlock() so the saved super block may contain
    // all the last qsBlock_config*() calls for a given block attribute.
    // If a qsBlock_config*() call is made for a block attribute more than
    // once, the older calls for a given attribute will not be saved; only
    // the last call of a given block attribute is saved.
    //
    bool saveAttributes;

    bool hasEpollBlock;
};




extern
void FreeGraphCommands(struct QsGraph *g);


static inline
void *GetBlock(uint32_t inCallbacks, uint32_t type,
        enum QsBlockType bType) {
    struct QsWhichBlock *wb = pthread_getspecific(blockKey);
    if(!wb) return 0;

    struct QsBlock *b = wb->block;
    if(!b) return 0;

    if(type)
        ASSERT(type & b->type,
                "Bad block \"%s\" calling this function",
                b->name);
    if(bType)
        ASSERT(bType == b->type);

    DASSERT(b->graph);

    DASSERT(wb->block->name);
    if(inCallbacks)
        ASSERT(inCallbacks & wb->inCallback,
                "Block \"%s\" is not in correct callback",
                wb->block->name);

    return b;
}


static inline
struct QsModule *Module(const struct QsBlock *b) {

    DASSERT(b);
    switch(b->type) {
        case QsBlockType_simple:
            return &((struct QsSimpleBlock *) b)->module;
        case QsBlockType_super:
            return &((struct QsSuperBlock *) b)->module;
        default:
            ASSERT(0);
            return 0;
    }
    return 0;
}


static inline 
struct QsModule *Get_Module(struct QsBlock *b) {

    DASSERT(b);

    if(b->type == QsBlockType_simple)
        return &((struct QsSimpleBlock *) b)->module;
    if(b->type == QsBlockType_super)
        return &((struct QsSuperBlock *) b)->module;
    ASSERT(0, "Write more code here");
    return 0;
}


static inline
struct QsModule *GetModule(uint32_t inCallbacks, enum QsBlockType type) {

    struct QsBlock *b = GetBlock(inCallbacks, QS_TYPE_MODULE, 0);
    if(b->type == QsBlockType_simple)
        return &((struct QsSimpleBlock *) b)->module;
    ASSERT(b->type == QsBlockType_super);
    return &((struct QsSuperBlock *) b)->module;
}


extern
struct QsDictionary *blocksPerProcess;

extern
pthread_mutex_t gmutex;

extern
struct QsDictionary *graphs;

extern
uint32_t numGraphs;

extern
uint32_t parameterQueueLength;


extern
void _qsGraph_destroy(struct QsGraph *g);


extern
void qsGraph_threadPoolHaltLock(struct QsGraph *g, struct QsThreadPool *tp);


extern
void qsGraph_threadPoolHaltUnlock(struct QsGraph *g);


extern
void qsGraph_flatten(struct QsGraph *g);


extern
void CleanupQsGetMemory(struct QsGraph *g);

