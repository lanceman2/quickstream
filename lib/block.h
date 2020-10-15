
struct QsDictionary;
struct QsApp;
struct QsGetter;
struct QsSetter;
struct QsBlock;


#define QS_IS_SUPERBLOCK  (001)



struct QsBlock {

    // The app that created/loaded this block and owns this block.
    struct QsApp *app;

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

    // QS_IS_SUPERBLOCK  (001)
    //
    uint32_t flags;
};


struct QsSimpleBlock {

    // This is the "real" block that has the work() callback an so on.

    // Inherit block.
    struct QsBlock block;
};


struct QsSuperBlock {

    // The super block loads other blocks and sets up parameter and 
    // input/output connections.

    // Inherit block.  This block is kind-of like a proxy for setting up
    // parameter and input/output connections to the "real" blocks that do
    // the work, in addition to connections that go to this block.
    struct QsBlock block;

    // Blocks that this super block is proxying connections for this list
    // of blocks:
    struct QsDictionary *blocks;

};


