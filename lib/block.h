
struct QsDictionary;
struct QsApp;
struct QsGetter;
struct QsSetter;
struct QsBlock;


#define QS_IS_SUPERBLOCK  (001)



struct QsBlock {

    // The app that created/loaded this block and owns this block.
    struct QsApp *app;

    // block name that is unique to this block for all block in app.
    const char *name;

    void *dlhandle; // from dlopen()

    // The threadPool that can run this blocks work()
    struct QsThreadPool *threadPool;

    // Some callbacks like getConfig(), construct() and destroy() we
    // do not same a pointer to, and dlsym() just before we call them.
    //
    // TODO: we may not be improving performance by saving pointers to
    // these functions, except for work() which is called in a tight loop.
    //
    // Pointers to optional callbacks from the DSO they are 0 if they are
    // not present.
    //
    int (*preStart)(struct QsBlock *b,
        uint32_t numInputs, uint32_t numOutputs);
    int (*postStart)(struct QsBlock *b,
        uint32_t numInputs, uint32_t numOutputs);

    int (*preStop)(struct QsBlock *b,
        uint32_t numInputs, uint32_t numOutputs);
    int (*postStop)(struct QsBlock *b,
        uint32_t numInputs, uint32_t numOutputs);

    int (* start)(uint32_t numInputs, uint32_t numOutputs);
    int (* stop)(uint32_t numInputs, uint32_t numOutputs);
    int (* work)(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs);
    // flush() gets called in place of work() when the stream is flushing;
    // that is it is called until len[] is all zeroed and there is no
    // filters feeding this block (filter).
    //
    // But if flush() is not present, then work() is called in it's place
    // in the same way that flush() would be called.
    int (* flush)(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs);
};



struct QsSimpleBlock {

    // Inherit block.
    struct QsBlock block;

    // lists of parameters owned by this block
    struct QsDictionary *getters;
    struct QsDictionary *setters;

    // We queue up setCallbacks that need to be called in the owner block
    // thread before and/or after the work() call.  This will queue up
    // many different parameters, but only the last element value is
    // queued for a given parameter, so each parameter can only have one
    // entry in this queue.
    //
    // This queue is added to by calls to qsParameterGetterPush() calls
    // from this or other blocks
    //
    // We do not expect a lot of inter-thread contention for this queue,
    // but we do need a mutex lock to access this queue.
    pthread_mutex_t mutex;

    // pointers to the setter parameter callbacks that are queued.
    // This block owns these setter parameters.
    struct QsSetter *first, *last;

    // next in Job queue that is waiting for a worker thread in the
    // ThreadPool.
    //
    struct QsBlock *next;

    // QS_IS_SUPERBLOCK  (001)
    //
    uint32_t flags;
};



struct QsSuperBlock {

    // Inherit block.
    struct QsBlock block;

    // The super block loads other blocks and sets up parameter and 
    // input/output connections between them.

    // Blocks that this super block is proxying connections for this array
    // of pointers to blocks
    uint32_t numBlocks;
    struct QsDictionary *blocks;
};

