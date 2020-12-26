
struct QsDictionary;
struct QsGraph;
struct QsThreadPool;
struct QsBlock;
struct QsSetter;

// For real stream I/O connections
struct QsInput;
struct QsOutput;
struct QsBuffer;

// For Super Block stream I/O connections
struct QsIn;
struct QsOut;

struct QsTrigger;


// These numbers are somewhat random in the hope to not accidentally match
// some other number.  We saw this as better than an enum.
//
#define _QS_IN_NONE             ((uint32_t) 0x2499f501)
// in declare()
#define _QS_IN_DECLARE          ((uint32_t) 0x38def4de)
// in construct()
#define _QS_IN_CONSTRUCT        ((uint32_t) 0xe583e10c)
// in destroy()
#define _QS_IN_DESTROY          ((uint32_t) 0x17fb51d9)
// in start()
#define _QS_IN_START            ((uint32_t) 0x9f1e3b8b)
// in stop()
#define _QS_IN_STOP             ((uint32_t) 0xdb85f71d)



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

    // We keep a doubly linked list of blocks in the owner graph in the
    // order in which they are loaded; so that we can iterate through them
    // in the order in which they are loaded and the reverse order in
    // which they loaded.  We call construct() and start() in forward
    // order.  We call stop() and destroy() in reverse order.
    //
    // Note: we also have a dictionary list of the blocks in the graph
    // so that we can lookup a block by name.
    //
    struct QsBlock *next, *prev;



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
    struct QsDictionary *getters; // getters and constants
    struct QsDictionary *setters; // setters

    // Since we can't allocate inputs in the declare() function we need to
    // store an list that tells us what input ports will use passThrough
    // ring buffers.
    //
    // numPassThroughs is the length of the realloc() allocated
    // passThroughs array.
    uint32_t numPassThroughs;
    struct QsPassThrough {
        uint32_t inputPortNum;
        uint32_t outputPortNum;
    } *passThroughs;

    // A doubly linked list of triggers.  The block is responsible for
    // cleaning up triggers when it is destroyed.  When the stream is
    // flowing the triggers provide job block callback functions.  This
    // list uses QsTrigger::next and QsTrigger::prev to make the list.
    //
    // The triggers go from the "waiting" list to the jobs list and then
    // back to waiting after their job is done.
    //
    struct QsTrigger *waiting;
    //
    // "jobs" is another doubly linked list of triggers.
    //
    // "jobs" that are ready to be worked on by a thread from the thread
    // pool.  firstJob will be the first job.  lastJob will be the last.
    //
    // We needed both ends of this list so that we can insert in the first
    // or the last element.  Stream triggers always go last in the jobs
    // list.
    struct QsTrigger *firstJob, *lastJob;

    // This is the trigger that is associated with stream input and
    // output if there is any.  streamTrigger stays 0 if there is no
    // stream input and output.
    //
    // The stream trigger seems to be special.  It always gets queued
    // last, so that other things can control the stream without delay.
    // It will call the users flow() and flush() functions.
    struct QsTrigger *streamTrigger;
    //
    bool userMadeTrigger;

    // The threadPool that can run this block's trigger actions.
    struct QsThreadPool *threadPool;

    // We keep a doubly linked list of blocks queued in the threadPool
    // using this "next" and "prev".  This said list changes as the stream
    // runs/flows.
    struct QsSimpleBlock *next, *prev;
    //
    // Is this simple block in the thread pool job queue?
    bool blockInThreadPoolQueue;


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


    // Some of the elements in the outputs[] array can be zeroed out with
    // the largest index (port) not being zeroed, while the connections
    // are being created; but, we do not let there be any zeroed entries
    // while the stream is flowing.
    uint32_t numOutputs; // number of output buffers we write to.
    // indexed by output port number 0,1,2,3,...
    struct QsOutput *outputs; // array of struct QsOutput
    //
    // numInputs is not so much as the number of inputs as it is the
    // number of elements in the inputs[] array.  Some of the elements in
    // the inputs[] array can be zeroed out with the largest index (port)
    // not being zeroed, while the connections are being created; but, we
    // do not let there be any zeroed entries while the stream is
    // flowing.
    uint32_t numInputs; // number of connected input filters feeding this.
    // array of struct QsInput indexed by input port number, 0,1,2,3,...
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

    // feederBlock is the block that is writing to this reader.
    struct QsSimpleBlock *feederBlock;

    // This buffer struct does not change at flow-time.
    struct QsBuffer *buffer;

    // This threshold will trigger a block->input() call, independent of
    // the other inputs.
    //
    // The block->flow() may just return without advancing any input or
    // output, effectively ignoring the flow() call like the threshold
    // trigger conditions where not reached.  In this way we may have
    // arbitrarily complex threshold trigger conditions.
    size_t threshold; // Length in bytes to cause flow() to be called.

    // The reading filter block promises to read some input data so long
    // as the buffer input length >= maxRead.
    //
    // It only has to read 1 byte to fulfill this promise, but so long as
    // the readable amount of data on this port is >= maxRead is will keep
    // having it's flow() called until the readable amount of data on this
    // port is < maxRead or an output buffer write pointer is at a limit
    // (???).
    //
    // This parameter guarantees that we can calculate a fixed ring buffer
    // size that will not be overrun.
    //
    size_t maxRead; // Length in bytes.

    // The input port number that this filter block being written to sees
    // in it's flow(,,portNum,) call.  This is an index into the
    // block->inputs[] array.
    uint32_t inputPortNum;
    // The output port number that the feedBlock uses as the output port
    // number that it is writing to.
    uint32_t outputPortNum;
};


struct QsOutput {  // points to reader filter blocks

    // Outputs (QsOutputs) are only accessed by the filter blocks
    // (QsSimpleBlocks) that own them.


    // The "pass through" buffers are a double linked list with the
    // "owner" buffer in the first one in the list.  The "owner" output
    // buffer has prev==0.
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
    // points to the next "pass through" down-stream output, if one is
    // present; else next is 0.
    struct QsOutput *next;


    // If prev is set this is a "pass through" and buffer points to the
    // "real" buffer that is the feed buffer, that feeds this pass through
    // buffer.
    //
    // This buffer struct does not change at flow-time.
    struct QsBuffer *buffer;

    // writePtr points to where to write next in mapped memory. 
    //
    // writePtr can only be read from and written to by the filter block
    // that feeds this output.  If the filter block that owns this output
    // can run flow() in multiple threads a filter block mutex lock is
    // required to read or write to this writePtr, but otherwise this is a
    // lock-less buffer when in the flow() call.
    //
    uint8_t *writePtr;

    // The block that owns this output promises to not write more than
    // maxWrite bytes to the buffer.  Also applies to pass-through
    // writing.
    //
    size_t maxWrite;


    // readers is where the output data flows to.
    //
    // Elements in this array are accessible from the QsSimpleBlock
    // (filter) but block->readers (not output->readers) are indexed by
    // the block's input port number.  This is readers of the current
    // output and the index number to this array is not necessarily
    // related to the reading block's input port number.
    //
    // Each element in this array may point to a different blocks input.
    //
    // This readers is a mapping from output to a block.  The
    // QsSimpleBlock structure has a readers array of pointers that is a
    // mapping from block input port to reader (or output buffer).  Yes,
    // it's complicated but essential, so draw a pointer graph picture.
    //
    // array of pointers to filter block input readers that this output
    // feeds.  The indexes to this array are not related to stream port
    // numbers.
    //
    // This inputs[] array of pointers needs to be allocated just before
    // running the flow, so we can be sure that the addresses of these
    // inputs is not being changed before running flow by editing the
    // stream connections.  To make this array we need to search all the
    // simple blocks in the graph.  See MakeOuputInputsArray() in
    // ringBuffers.c.
    struct QsInput **inputs;
    //
    uint32_t numInputs; // length of inputs (readers) array

    // flow() just returns 0 if a threshold is not reached.
};


struct QsBuffer {

    ///////////////////////////////////////////////////////////////////////
    // This structure stays constant while the stream is flowing.
    //
    // There are two adjacent memory mappings per buffer.
    //
    // The start of the first memory mapping is at end - mapLength.  We
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
