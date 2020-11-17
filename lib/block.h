
struct QsDictionary;
struct QsGraph;
struct QsBlock;
struct QsSetter;

// For real stream I/O connections
struct QsInput;
struct QsOutput;
struct QsBuffer;

// For Super Block stream I/O connections
struct QsIn;
struct QsOut;



#define _QS_IN_NONE             ((uint32_t) 0x2499f504)

#define _QS_IN_BOOTSTRAP        ((uint32_t) 0x38def4de)
// construct()
#define _QS_IN_CONSTRUCT        ((uint32_t) 0xe583e10c)
// and destroy()
#define _QS_IN_DESTROY          ((uint32_t) 0x17fb51d9)


struct QsBlock {

    // QsBlock is the base class for QsSimpleBlock and QsSuperBlock.  A
    // pointer to a struct QsBlock is the opaque interface that the API
    // (application user interface) user uses to access both QsSimpleBlock
    // and QsSuperBlock.  The quickstream API user never sees
    // QsSimpleBlock or QsSuperBlock.  Hence struct QsBlock is an
    // interface wrapper to QsSimpleBlock and QsSuperBlock.  This leads to
    // a more seamless interface to quickstream blocks.  You use simple
    // blocks just like supper blocks; and that's very powerful.
    //
    // Counter to that, GNU radio blocks have their guts hanging out
    // all other the place.  I could go on about it, to no end.

    // The graph that created/loaded this block and owns this block.
    struct QsGraph *graph;

    // block name that is unique to this block for all block in graph.
    const char *name;

    // _QS_IN_NONE
    // _QS_IN_BOOTSTRAP
    // _QS_IN_CONSTRUCT
    // _QS_IN_DESTROY
    //
    uint32_t inWhichCallback;

    // dlhandle can be zero as a sign that block functions should not be
    // called any more; functions from the DSO, like start() and flow().
    void *dlhandle; // from dlopen()

    // The threadPool that can run this blocks flow()
    struct QsThreadPool *threadPool;

    // Some callbacks like boostrap(), construct() and destroy() we
    // do not same a pointer to, and dlsym() just before we call them.
    //
    // Pointers to optional callbacks from the DSO they are 0 if they are
    // not present.
    //
    //
    int (* start)(uint32_t numInputs, uint32_t numOutputs);
    int (* stop)(uint32_t numInputs, uint32_t numOutputs);


    // This just lets us know if the memory allocated is a struct
    // QsSimpleBlock or struct QsSuperBlock
    //
    bool isSuperBlock;
};


struct QsSimpleBlock {

    // Inherit block.
    struct QsBlock block;

    // lists of parameters owned by this block
    struct QsDictionary *getters;
    struct QsDictionary *setters;


    // The number of inputs can change before start and after stop, so can
    // maxThread in the stream and so jobs and these input() arguments are
    // all reallocated at qsStreamLaunch().
    //
    // The inputBuffers[i], inputLens[i] i=0,1,2,..N-1 numInputs pointer
    // values can only be changed in qsOutput() by the feeding filter
    // block's thread.  If the feeding filter block can have more than one
    // thread than the thread that is changing them will lock the output
    // mutex.
    //
    // The arguments to pass to the input() call.  All are indexed by
    // input port number
    void **inputBuffers; // allocated after start and freed at stop size_t
    size_t *inputLens;   // allocated after start and freed at stop

    // advanceLens is the length in bytes that the filter block advanced
    // the input buffers, indexed by input port number.
    size_t *advanceLens; // allocated after start and freed at stop
    //
    // outputLens from qsOuput() and qsGetOutputBuffer() calls from in
    // filter flow().   Length of this array is filter block numOutputs.
    size_t *outputLens; // amount output was advanced in input() call.

    // This will be the pthread_getspecific() data for each flow thread.
    // Each thread just calls the filter (QsSimpleBlock) flow() function.
    // When there is more than on thread calling a filter flow() we need
    // to know things, QsJob, about that thread in that flow() call.


    // Super blocks can't have a flow() or flush().  The super blocks
    // are not used while the stream is flowing.  The super blocks are
    // just flow-graph build-time and paused-time construct.
    //
    int (* flow)(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs);
    //
    // flush() gets called in place of flow() when the stream is flushing;
    // that is it is called until len[] is all zeroed and there is no
    // block feeding this block.
    //
    // But if flush() is not present, then flow() is called in it's place
    // in the same way that flush() would be called.
    int (* flush)(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs);

    uint32_t numOutputs; // number of connected filter blocks we write to.
    struct QsOutput *outputs; // array of struct QsOutput
    //
    //
    uint32_t numInputs; // number of connected input filters feeding this.
    struct QsInput *inputs; // array of struct QsInput

    // We queue up setCallbacks that need to be called in the owner block
    // thread before and/or after the flow() call.  This will queue up
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

    // pointers to the setter parameter callbacks that are queued.  This
    // block owns these setter parameters.
    struct QsSetter *first, *last;

    // next in Job queue that is waiting for a flower thread in the
    // ThreadPool.
    //
    struct QsBlock *next;
};


struct QsInput {
    
    // readPtr points to a location in the mapped memory, "ring buffer".
    //
    // After initialization, readPtr is only read and written by the
    // reading filter block.  We use a reading filter mutex for
    // multi-thread filters flow()s, but otherwise reading/writing the
    // buffer is lock-less.
    //
    uint8_t *readPtr;

    // readLength is the number of bytes to the write pointer at this
    // pass-through level.  Accessing readLength requires a stream mutex
    // lock.
    size_t readLength;

    // The filter block that is reading.
    struct QsSimpleBlock *block;

    // feedBlock is the block that is writing to this reader.
    struct QsBlock *feedBlock;

    struct QsBuffer *buffer;

    // This threshold will trigger a block->input() call, independent of
    // the other inputs.
    //
    // The block->input() may just return without advancing any input or
    // output, effectively ignoring the input() call like the threshold
    // trigger conditions where not reached.  In this way we may have
    // arbitrarily complex threshold trigger conditions.
    size_t threshold; // Length in bytes to cause input() to be called.

    // The reading filter block promises to read some input data so long
    // as the buffer input length >= maxRead.
    //
    // It only has to read 1 byte to fulfill this promise, but so long as
    // the readable amount of data on this port is >= maxRead is will keep
    // having it's input() called until the readable amount of data on
    // this port is < maxRead or an output buffer write pointer is at a
    // limit (???).
    //
    // This parameter guarantees that we can calculate a fixed ring buffer
    // size that will not be overrun.
    //
    size_t maxRead; // Length in bytes.

    // The input port number that this filter block being written to sees
    // in it's input(,,portNum,) call.
    uint32_t inputPortNum;
};


struct QsOutput {  // points to reader filter blocks

    // Outputs (QsOutputs) are only accessed by the filter blocks
    // (QsSimpleBlocks) that own them.


    // The "pass through" buffers are a double linked list with the "real"
    // buffer in the first one in the list.  The "real" output buffer has
    // prev==0.
    //
    // If this a "pass through" output prev points to the in another
    // filter block output that this output uses to buffer its' data.
    //
    // So if prev is NOT 0 this is a "pass through" output buffer and prev
    // points toward the up-stream origin of the output thingy; if prev is
    // 0 this is not a "pass through" buffer.
    //
    struct QsOutput *prev;
    //
    // points to the next "pass through" output, if one is present; else
    // next is 0.
    struct QsOutput *next;


    // If prev is set this is a "pass through" and buffer points to the
    // "real" buffer that is the feed buffer, that feeds this pass through
    // buffer.
    //
    struct QsBuffer *buffer;

    // writePtr points to where to write next in mapped memory. 
    //
    // writePtr can only be read from and written to by the filter block
    // that feeds this output.  If the filter block that owns this output
    // can run input() in multiple threads a filter block mutex lock is
    // required to read or write to this writePtr, but otherwise this is a
    // lock-less buffer when in the input() call.
    //
    uint8_t *writePtr;

    // The block that owns this output promises to not write more than
    // maxWrite bytes to the buffer.
    //
    size_t maxWrite;

    // This is the maximum of maxWrite and all reader maxRead for
    // this output level in the pass-through buffer list.
    //
    // See the function: GetMappingLengths() in buffer.c
    //
    // This is used to calculate the ring buffer size and than
    // is used to determine if writing to the buffer is blocked
    // by buffer being full.
    size_t maxLength;

    // readers is where the output data flows to.
    //
    // Elements in this array are accessible from the QsSimpleBlock
    // (filter) but block->readers (not output->readers) are indexed by
    // the block's input port number.  This is readers of the current
    // output and the index number to this array is likely not related to
    // the reading block's input port number.
    //
    // Each element in this array may point to a different block.
    //
    // This readers is a mapping from output to a block.  The QsSimpleBlock
    // structure has a readers array of pointers that is a mapping from
    // block input port to reader (or output buffer).  Yes, it's
    // complicated but essential, so draw a pointer graph picture.
    //
    struct QsInput *inputs; // array of filter block input readers.

    uint32_t numInputs; // length of inputs (readers) array

    // flow() just returns 0 if a threshold is not reached.
};


struct QsBuffer {

    ///////////////////////////////////////////////////////////////////////
    // This structure stays constant while the stream is flowing.
    //
    // There are two adjacent memory mappings per buffer.
    //
    // The start of the first memory mapping is at end + mapLength.  We
    // save "end" in the data structure because we use it more in looping
    // calculations than the "start" of the memory.
    //
    uint8_t *end; // Pointer to end of the first mmap()ed memory.

    //
    // These two elements make it a circular buffer or ring buffer.  See
    // makeRingBuffer.c.  The mapLength is the most the buffer can hold to
    // be read by a filter block.  The overhangLength is the length of the
    // "wrap" mapping that is a second memory mapping just after the
    // mapLength mapping, and is the most that can be written or read in
    // one filter transfer "operation".
    //
    size_t mapLength, overhangLength; // in bytes.
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
