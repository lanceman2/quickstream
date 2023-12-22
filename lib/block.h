
//#define QS_MODULE_DLOPEN_FLAGS  (RTLD_LAZY | RTLD_GLOBAL | RTLD_DEEPBIND)
#define QS_MODULE_DLOPEN_FLAGS  (RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND)

#define MAX_BUILTIN_SYMBOL_LEN  48


// Block callback function bit mask markers:
//
#define CB_NONE             ((uint32_t) 000000)
// load
#define CB_DECLARE          ((uint32_t) 000001)
// unload
#define CB_UNDECLARE        ((uint32_t) 000002)
#define CB_CONFIG           ((uint32_t) 000004)
#define CB_CONSTRUCT        ((uint32_t) 000010)
#define CB_DESTROY          ((uint32_t) 000020)
#define CB_START            ((uint32_t) 000040)
#define CB_STOP             ((uint32_t) 000100)
#define CB_FLOW             ((uint32_t) 000200)
#define CB_FLUSH            ((uint32_t) 000400)
#define CB_INTERBLOCK       ((uint32_t) 001000)
#define CB_SET              ((uint32_t) 002000)

#define CB_ANY              (CB_DECLARE|\
                            CB_UNDECLARE|\
                            CB_CONFIG|\
                            CB_CONSTRUCT|\
                            CB_DESTROY|\
                            CB_START|\
                            CB_STOP|\
                            CB_FLOW|\
                            CB_FLUSH|\
                            CB_INTERBLOCK|\
                            CB_SET)



// QsBlock is seen as an opaque struct to the libquickstream.so API user.
// The API user just gets a pointer to the struct QsBlock.
//
// The graph mutex protects access all the data in this structure except
// type and graph which never change.
//
struct QsBlock {

    // The type of a block never changes so there is no mutex protection
    // for it.
    enum QsBlockType type;

    char *name;

    // This block is owned by this graph.
    struct QsGraph *graph;

    // If the parent is destroyed than this block is destroyed.  That is
    // to say this parent owns this block, so long as it exists.
    //
    // Only a top block (graph) has parentBlock == 0
    //
    struct QsParentBlock *parentBlock;

    // The parent block may have other children.  We use nextSibling and
    // prevSibling to make a doubly linked list of the parents children
    // with the first child at parentBlock->firstChild and the last child
    // at parentBlock->lastChild.
    struct QsBlock *nextSibling, *prevSibling;

    // Passed to block callback functions.
    void *userData;
};


// A block that can have jobs.
//
struct QsJobsBlock {

    struct QsBlock block; // inherit block

    // List of all jobs that this block owns.  This list uses QsJob::p and
    // QsJob::n to make a doubly listed list.  jobsStack points to the top
    // of a stack data structure.  jobsStack->n is the next one in the
    // stack if it exists.
    //
    // protected by graph mutex.
    //
    struct QsJob *jobsStack;

    // All the data below (in this structure) are protected by a thread
    // pool mutex, and/or thread pool halt locks.  It's fucking
    // complicated.


    // For the thread pool "queue" of blocks.  A doubly listed list of
    // blocks that are in the thread pool "queue".
    //
    // Note: there is a queue of blocks in the thread pool and a queue of
    // jobs in the blocks.  There's a thread pool queue of block queues.
    // Two layers of queues.  It's what naturally emerged.  It had to be
    // something like that.
    //
    struct QsJobsBlock *next, *prev;


    // The block's queue of jobs waiting to be worked on:
    //
    // "first" and "last" make the queue, along with "firstStream".
    //
    // This queue uses QsJob::prev and QsJob::next to make a doubly listed
    // list.  This will be the "queued" jobs that are always in
    // QsJobsBlock::jobsStack.
    //
    // There are 3 points in this job queue:
    //
    //  "first" and "last" which are the first and last in the queue, and
    //
    //  "firstStream" is at or between "first" and "last" or zero.
    //
    //
    // If there is a stream job in the block's job queue, there will be
    // only stream jobs from "firstStream" to and including "last".
    //
    // If there is no stream jobs in the block's job queue, "firstStream"
    // will be equal to zero.
    //
    // If there are only stream jobs in the block's job queue "firstStream"
    // will be equal to "first".
    //
    // You see the "stream jobs" are special in that they are queued in
    // this block's job queue with a lower priority then all other jobs.
    // This lower priority makes it so that stream jobs always happen
    // after other jobs that may wish to control the stream.
    //
    // TODO: add a simple stream tag example and test.
    //
    struct QsJob *first, *last, *firstStream;

    // What happens is: jobs can see that the work is needed many times
    // before the work() is called; so we need to stop it from being
    // queued more than once.
    //
    bool inQueue;

    // The block will not be queued in the thread pool's queue when this "busy"
    // flag is set.  So long as this "busy" flag is set we do not queue
    // this block into the thread pool queue.
    //
    // We can also use this "busy" signal to have it not be queued in the
    // thread pool.  Maybe "busy" should be "doNotQueue".
    //
    bool busy;


    // A list of thread pools that have jobs that may access queue this
    // block and the jobs in it.
    //
    // Each element is a CRBNode and is allocated as a QsThreadPoolNode.
    //
    // All this red black tree node (struct QsThreadPoolNode) memory
    // allocations are owned by this Job Block.
    //
    CRBTree threadPools;


    // This block will run jobs/events in this thread pool.
    //
    // So long as the block exists there must be an assigned thread pool.
    //
    // The assigned thread pool can change.
    struct QsThreadPool *threadPool;
};


struct QsThreadPoolNode {

    struct QsThreadPool *threadPool;

    // How many times it's been added with Block_addThreadPool().
    uint32_t refCount;

    // This struct is a node in a red black tree.
    CRBNode node;
};


struct QsPortDicts {

    // 1. For super blocks these dictionaries are aliases to simple block
    //    ports.
    //
    // 2. For simple blocks the entries are the ports in the simple
    //    block.
    //
    // All the key/value  values are pointers to struct QsPort
    //
    // struct QsParameter inherits struct QsPort
    //
    // struct QsSetter inherits struct QsParameter
    // struct QsGetter inherits struct QsParameter
    // struct QsInput inherits struct QsPort
    // struct QsOutput inherits struct QsPort
    //
    //
    // For QsGraph making connection aliases when and if this graph is
    // saved as a super block.  Like in struct QsModule.
    //
    struct QsDictionary *setters; // value = pointer to struct QsSetter
    struct QsDictionary *getters; // value = pointer to struct QsGetter
    struct QsDictionary *inputs;  // value = pointer to struct QsInput
    struct QsDictionary *outputs; // value = pointer to struct QsOutput
};


struct QsModule {

    // There are two kinds of modules "built in" and "loaded DSO (Dynamic
    // Shared Object)".
    //
    // dlhandle will be 0 for a built-in module.
    //
    void *dlhandle;

    // As passed to qsGraph_createBlock().  The user sees this as what
    // the block is.
    //
    // This is likely not a full path.
    //
    // 1. For a built-in block this is the prefix for a series of function
    //    symbols like "File"; so as to find symbol "File_declare" for the
    //    function File_declare() that was in a source file
    //    builtInBlocks/File/File.c.
    //
    // Note: the sub-directory File/ is only used to help classify the
    // block module; but the word "File" must be unique to that built-in
    // block module.  DSO block modules do not have such a naming
    // restriction; as they can be in files with file names that have
    // non-unique base file names; only the full path needs to be
    // unique.
    //
    // 2. For DSO (dynamic shared object) file block modules this is the
    //    file name that is a relative or full path to the DSO file.  The
    //    environment variable QS_BLOCK_PATH helps us find the DSO files
    //    in addition to the code DSO block modules in the relative path
    //    lib/quickstream/blocks/.
    //
    char *fileName;

    // For a DSO path is the full path to the DSO.  Though the path of
    // the file that is dlopen()ed may be a temporary file that is a copy
    // of the DSO at fullPath.
    //
    // For a built-in block module fullPath is 0.
    //
    char *fullPath;


    // Simple blocks and super blocks have configurable attributes:
    struct QsDictionary *attributes;
    //
    // We keep track of data for qsParseSizet() (and like functions) when
    // configuring an attribute in the block code.  In the block code
    // in a configure callback we track the state of the qsParse*()
    // functions with these pointers and counters:
    //
    const char * const *argv;
    int argc;
    int i;

    // 1. For super blocks these dictionaries are aliases to simple block
    //    ports.
    //
    // 2. For simple blocks the entries are the ports in the simple
    //    block.
    //
    struct QsPortDicts ports;
};


// QsSimpleBlock is a leaf node in the block family tree data structure.
//
struct QsSimpleBlock {

    struct QsJobsBlock jobsBlock; // inherit QsJobsBlock


    // If we have any stream inputs and/or outputs "streamJob" is
    // allocated otherwise it is 0.
    //
    // This may seem a little redundant given we have pointers to all jobs
    // in the jobsBlock part of this structure.  But, it is needed since
    // the data structures in it contains data that will not be in the
    // jobs list of jobsBlock when the stream is not flowing.  The stream
    // job needs to get many parts reallocated as the stream starts and
    // stops.  We do not reallocate all the data structures pointed to by
    // streamJob every time we restart the stream.
    //
    // Since not all blocks have stream input and output, we don't
    // make this a built-in as in: struct QsStreamJob streamJob;
    // without the "*", and the stream job structure is quite large.
    //
    struct QsStreamJob *streamJob;


    // If set, The run file is a place look for block callbacks:
    // construct(), start(), flow(), flush(), stop(), and destroy().
    //
    // The purpose of the "run file" is to keep the loaded code small
    // until some block callbacks are called.  Like if a block links with
    // a bunch of libraries that are not needed until some block callbacks
    // are called.  The path of the run file is in the directory where
    // the block was loaded from.  Run file filenames start with an
    // underscore, "_".  Hence, block filenames may not start with an
    // underscore.
    //
    // Looks like built-in blocks can have run files along with DSO
    // (dynamic shared object) blocks.
    //
    char *runFile;
    void *runFileDlhandle;
    bool runFileFailed; // Set if we failed to load the run file DSO.
    // if true we do load a copy of the run file DSO.
    bool runFileLoadCopy;

    // startChecked is set if there is ever an attempt get the start and
    // stop functions for this block; we use it to try to call destroy()
    // and construct() too.
    bool startChecked;
    //
    // So we do not call stop() for a block that did not get start()
    // called do to a start failure.
    bool started;
    //
    // construct() or start() returned -1 so do not call the rest of
    // the stream start sequence for this block, just for this stream
    // start/stop cycle.
    //
    bool donotFinish;
    //
    // Callback functions that maybe called more than once:
    //
    // Note: the block does not need a stream job, or any stream
    // connections, to have start(), or stop() called.
    //
    int (*start)(uint32_t numInputs, uint32_t numOutputs, void *userData);
    int (*stop)(uint32_t numInputs, uint32_t numOutputs, void *userData);


    struct QsModule module;
};


struct QsParentBlock {

    struct QsBlock block; // inherit QsBlock

    // A parent block owns other blocks.
    struct QsBlock *firstChild, *lastChild;
};


// QsSuperBlock is a parent node in the block family tree data structure,
// but it is not required to have children.  The number of children can
// change over time.
//
struct QsSuperBlock {

    struct QsParentBlock parentBlock; // inherit QsParentBlock

    struct QsModule module;
};


// Used in worker threads from pthread_getspecific(threadPoolKey)
struct QsWhichJob {

    // Which job changes.
    struct QsJob *job;

    // The thread pool stays fixed.
    struct QsThreadPool *threadPool;
};


struct QsWhichBlock {

    // Which block changes.
    struct QsBlock *block;

    // Compare with block function callback bit mask flags like
    // CB_DECLARE.
    uint32_t inCallback;
};



// Sets a QsWhichJob when j->work() is called.
extern
pthread_key_t threadPoolKey;

// To find the block when the block code uses the libquickstream.so API.
// For example a block calls qsSetNumInputs() from in the function
// declare() in that block code.
extern
pthread_key_t blockKey;


extern
void qsBlock_init(struct QsGraph *g, struct QsBlock *b,
        struct QsBlock *p/*parentBlock*/,
        struct QsThreadPool *tp, enum QsBlockType type,
        const char *name, void *userData);

extern
void qsBlock_cleanup(struct QsBlock *b);


static inline
struct QsJob *GetJob(void) {

    struct QsWhichJob *wj = pthread_getspecific(threadPoolKey);
    ASSERT(wj, "pthread_getspecific() for thread pool key failed");
    DASSERT(wj->job);
    return wj->job;
}



// Managing a list of thread pools for a jobs block.  The block's thread
// pool list is a red black tree using the c-rbtree API (application
// programming interface).  The list is used to make
// qsBlock_threadPoolHaltLock() to halt just the thread pools that are
// needed to edit the block and it's jobs; which is the same as making
// data flow connections between blocks (connecting jobs in the blocks).
// To edit a block you need to halt the thread pool that runs that block
// and all the thread pools that that block may queue jobs to; so we need
// to keep a list of thread pools for each block.
//
// It's best to draw pictures of blocks and jobs in blocks and lines
// between the jobs in the blocks.  It's fun with graph theory.  Once you
// draw pictures it's easy.  Before you draw simple pictures it's a big
// WTF mess.  Blocks and the affiliated thread pools due to connections
// between jobs in the blocks; i.e. a fucking mess unless you draw
// simple pictures.
//
// These 3 functions are our little wrapper sub API that manages the
// QsJobsBlock::threadPools list.
//
// Recap: Manage lists of affiliated thread pools that are due to
// connections between jobs in the blocks.
//
extern
void Block_addThreadPool(struct QsJobsBlock *b, struct QsThreadPool *tp);
extern
uint32_t qsBlock_threadPoolHaltLock(struct QsJobsBlock *b);
extern
void Block_removeThreadPool(struct QsJobsBlock *b,
        struct QsThreadPool *tp);


static inline void FreeString(char **str) {
    if(*str) {
        DZMEM(*str, strlen(*str));
        free(*str);
#ifdef DEBUG
        *str = 0;
#endif
    }
}


#define NotWorkerThread()\
    do {\
            ASSERT(pthread_getspecific(threadPoolKey) == 0,\
                "quickstream worker threads may not call this");\
    } while(0)


// Returns a pointer to a function that is either from a
// block DSO (via dlhandle) or from a built-in block.
extern
void *GetBlockSymbol(void *dlhandle,
        const char *prefix, const char *symbolName);


// Called before the libquickstream API calls a block callback
//
// If stackSave is set, it will store the current block callback info that
// is currently active.  You see, a block callback can call the API that
// can call another block callback and so on.  This helps make the what
// ever the API function is be re-entrant (calling itself again and
// again); that is the API function can call itself indirectly from
// itself many times.
//
// Some libquickstream API functions do not have to be re-entrant, hence
// stackSave is optional.  Looks like most have to be re-entrant.
static inline void
SetBlockCallback(struct QsBlock *b, uint32_t inCallback,
            struct QsWhichBlock *stackSave) {

    DASSERT(b);
    DASSERT(inCallback);
    DASSERT(stackSave);

    // TODO: This will leak memory if many threads call this.
    // Currently this memory is cleaned up in the library destructor
    // assuming that the same thread call this calls the library
    // destructor.  We need to add a pthread key cleanup thingy.
    // I think it's in the man pages.  RTFM.

    struct QsWhichBlock *w = pthread_getspecific(blockKey);

    if(!w) {
        w = calloc(1, sizeof(*w));
        ASSERT(w, "calloc(1,%zu) failed", sizeof(*w));
        CHECK(pthread_setspecific(blockKey, w));
    }

    if(stackSave)
        // Save the w value.
        memcpy(stackSave, w, sizeof(*w));

    // A block that is in a callback function call cannot call its own
    // callbacks from the API.  Hurts my head to think about.  Example: a
    // super block cannot configure itself; that would make the
    // quickstream module scheme unusable.  We make it a ASSERT() and not
    // DASSERT() so that it is always an API user error.
    //
    // TODO: How do we stop bad block writers from making subservient
    // blocks that do their dirty work; thereby making blocks that take
    // away controls from the graph builder?
    //
    ASSERT(w->block != b,
            "Looks like you have a module trying to control itself; "
            "consider writing a higher level super block to do that.");

    // Set the new value.
    w->block = b;
    w->inCallback = inCallback;
}


// Called after the libquickstream API calls a block callback; to help
// make the API function re-entrant.  See SetBlockCallback().
static inline void
RestoreBlockCallback(const struct QsWhichBlock *stackSave) {

    struct QsWhichBlock *w = pthread_getspecific(blockKey);
    DASSERT(w);

    if(stackSave)
        memcpy(w, stackSave, sizeof(*w));
    else
        memset(w, 0, sizeof(*w));
}


extern
void *GetRunFileDlhandle(struct QsSimpleBlock *b);


extern
bool qsGraph_canConnect(const struct QsPort *p1, const struct QsPort *p2);
