
struct QsDictionary;
struct QsApp;
struct QsBlock;
struct QsParameter;
struct QsSetter;

// For real stream I/O connections
struct QsInput;
struct QsOutput;

// For Super Block stream I/O connections
struct QsIn;
struct QsOut;



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


    // QS_IS_SUPERBLOCK  (001) or is QsSimpleBlock
    //
    uint32_t flags;
};



struct QsSimpleBlock {

    // Inherit block.
    struct QsBlock block;

    // lists of parameters owned by this block
    struct QsDictionary *getters;
    struct QsDictionary *setters;

    // Super blocks can't have a work() or flush().  The super blocks
    // are not used while the stream is flowing.  The super blocks are
    // just flow-graph build-time construct.
    //
    int (* work)(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs);
    //
    // flush() gets called in place of work() when the stream is flushing;
    // that is it is called until len[] is all zeroed and there is no
    // filters feeding this block (filter).
    //
    // But if flush() is not present, then work() is called in it's place
    // in the same way that flush() would be called.
    int (* flush)(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs);



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
};



struct QsSuperBlock {

    // The super block loads other blocks and sets up parameter and 
    // input/output connections between them from both inside this block
    // and outside this block.
    //
    // Inner blocks contained in this block may also be super blocks.

    // Inherit block.
    struct QsBlock block;

    // Exposed parameters.  Only exposed parameters can be connected to
    // outside the super block.  The super block builders must explicitly
    // exposes parameters.  The super block does not make parameters of
    // it's own.  It's just a shell, an intermediary to parameters
    // inside blocks inside itself.
    //
    // A mapping from this super block into the inner simple block
    // parameters.
    //
    // This super block is named from above by the builder that loaded
    // this super block.
    //
    // A hierarchy of names is needed to keep from having parameters owned
    // by inner blocks from having names that overlap with other
    // parameters from other inner blocks.
    //
    // The parameters may be constant, getter, or setter in any inner
    // block.
    //
    struct QsDictionary *parameters; // exposed

    // Blocks that this super block is proxying connections for this array
    // of pointers to blocks
    uint32_t numBlocks;
    struct QsBlocks **blocks;


    // This is not GNUradio.  Quickstream lets the blocks decide at
    // bootstrap or flow start wither or not there are the correct number
    // of input/output stream flow connections.  The port number is just
    // an index into an array.

    // Array of inputs.  The array index is the port number.
    uint32_t numIns;
    struct QsIn *ins;

    // Array of outputs.  The array index is the port number.
    uint32_t numOuts;
    struct QsIn *outs;
};


struct QsIn {

    // Each input port can only have one connection from an output port.

    // The block feeding this input.
    struct QsBlock *block;
    uint32_t portNum; // array index
};


struct QsOut {

    // Each stream output may connect to any number of input ports.

    uint32_t numPorts;

    struct QsBlock **blocks;
    uint32_t *inputPorts; // array input index
};
