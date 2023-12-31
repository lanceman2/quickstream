

struct QsBuffer {

    // This structure stays constant while the stream is flowing.
    //
    // There are two adjacent memory mappings per buffer.
    //
    // The start of the first memory mapping is at end - mapLength.  We
    // save "end" in the data structure because we use it more in looping
    // calculations than the "start" of the memory.
    //
    // end points between the two memory mappings.
    //
    void *end; // Pointer to end of the first mmap()ed memory.

    //
    // These two elements make it a circular buffer or ring buffer.  See
    // makeRingBuffer.c.  The mapLength is the most the buffer can hold to
    // be read by a block.   mapLength is not trivial to calculate when
    // this is part of pass-through buffer chain.  We find the maximum of
    // the lengths by looping over all paths through the maximum write and
    // maximum read lengths, through the blocks.
    //
    // The overhangLength is the length of the "wrap" mapping that is a
    // second memory mapping just after the mapLength mapping, and is the
    // most that can be written or read in one filter block transfer
    // operation.  This is less than or equal to mapLength.
    //
    // these get rounded to the nearest page size.
    //
    size_t mapLength, overhangLength; // in bytes.
};


struct QsInput {

    struct QsPort port; // inherit port

    // The index of this input in the stream job inputs[].
    uint32_t portNum;

    // An input can connect to only one output.
    struct QsOutput *output;

    // Marks that we will have a pass through to this output if we
    // get this output (passThrough) and input connected.
    //
    // Note: this can change while the stream is running, but it will
    // not effect the current running stream cycle; it will effect the
    // next stream run.
    //
    // passThrough is only accessed by the "master thread" (the thread
    // with the graph mutex lock), and not accessed by thread pool worker
    // threads.  See QsOutput::passThrough.
    //
    struct QsOutput *passThrough;

    // The reading block promises to read some input data, on some
    // input port, so long as the buffer input length >= maxRead on all
    // input ports and that the outputs are not clogged.  The value is
    // different for each port.
    //
    // Clogged outputs are just outputs with not all output buffer
    // write lengths not greater than or equal to the output maxWrite.
    //
    // A block is not required to keep this promise, but the flow may be
    // stalled if the promise is not upheld by the block.  It's more of a
    // "do this or else" thing, than a promise.
    //
    // Set with qsSetInputMax() at stream start.
    //
    size_t maxRead;
    //
    // This will be the value of maxRead for the next qsGraph_start().
    size_t nextMaxRead;


    // NOTE: If the output that feeds this input is Flushing (isFlushing
    // is set), then the blocks flow() or flush() function can't expect
    // to be able to read maxRead on it's input port.


    // readLength is the number of bytes to the write pointer from the
    // read pointer at this pass-through level, on the ring buffer; it's
    // the amount that can be read and may exceed the maximum that we let
    // the block read.
    //
    // Accessing readLength requires mutex locks of all simple blocks
    // involved ...
    //
    // read and write pointers are in the stream job: inputBuffers[] and
    // outputBuffers[].  This input is in the stream job too.
    //
    // readLength can get larger while flow() is being called.
    //
    // readLength may seem superfluous.  We could have just as well
    // calculated the difference between the write pointer and the read
    // pointer to get it's value, but it made coding easier; because we
    // needed to store buffer lengths at a time when we cannot move the
    // pointers while the flow() callback is being called.  readLength is
    // a variable that is shared between blocks and threads; where as the
    // read and write pointers are not.  The read and write pointers are
    // moved by the stream job (and block) that own them, and they are not
    // shared between blocks.
    //
    size_t readLength;
};


struct QsOutput {

    struct QsPort port; // inherit port

    // The index of this output in the stream job outputs[].
    uint32_t portNum;

    // Array of pointers to inputs that connect to this output.
    struct QsInput **inputs;
    uint32_t numInputs;

    // Marks that we will have a pass through to this input if we get this
    // output and input (passThrough) connected.
    //
    // passThrough is only accessed by the "master thread" (the thread
    // with the graph mutex lock), and not accessed by thread pool worker
    // threads.  It is only accessed when it is set or unset in
    // qsMakePassThroughBuffer() and at qsGraph_start(); the effect is
    // that there is a doubly listed list outputs formed at
    // qsGraph_start().  See QsInput::passThrough.
    //
    struct QsInput *passThrough;

    // The block can write this much in every flow() or flush() call; that
    // is guaranteed by the libquickstream.so API.
    //
    // The block writer can never write more than the buffer length passed
    // to the flow() and flush() calls.
    //
    size_t maxWrite;
    // nextMaxWrite is the value for maxWrite used in the next
    // qgGraph_start() and running of the stream.  We have this so the
    // block can set qsSetOutputMax() at any time.  The block does not
    // have to wait until the start() callback to call qsSetOutputMax()
    // and that save the block from having to store a block local variable
    // that will be the next maxWrite value.
    //
    // Because the block's thread pool is halted when this is accessed by
    // the graph running thread (master thread) nextMaxWrite can be set in
    // any block callback.
    size_t nextMaxWrite;

    // The maximum of all input maxRead for inputs that this output feeds.
    size_t maxMaxRead;

    // The "pass through" buffers are a doubly linked list with the
    // "owner" buffer is the first one in the list.
    //
    // The buffer "owner" output has prev == 0.  It's the start of the
    // buffer writers.
    //
    // So if prev is NOT 0 this is a "pass through" output buffer and prev
    // points toward the up-stream origin of the output thingy.
    //
    // This is set just before the stream is running so we do not have to
    // re-find the output it points to if output arrays are reallocated.
    struct QsOutput *prev;
    //
    // points to the next "pass through" down-stream output, if one is
    // present; else next is 0.
    //
    // This is set just before the stream is running so we do not have to
    // re-find the output it points to if output arrays are reallocated.
    struct QsOutput *next;

    // There may be many inputs and outputs pointing to this ring buffer;
    // given we have "pass-through" buffers.  Pass-through buffers allow
    // us to pass data through a block without having to copy memory to
    // another buffer.  We do it using a longer ring buffer that runs
    // through a middle block to another blocks input.  There is no limit
    // as to how many blocks a ring buffer may pass-through.
    //
    // We do not fork the buffer to pass-through two or more blocks at one
    // time.  It's not obvious what that even means.
    //
    // Only outputs can own the buffers because outputs can have many
    // inputs connected to them.
    //
    // The first block output owns this buffer.  Owns means that it must
    // clean it up.  If it is not a pass-through buffer than that's the
    // only output, and there are no pass-through outputs.
    struct QsBuffer *buffer;

    // Each output may be declared as done having data written to it,
    // for a given stream run.  The isFlushing state will flow through
    // the stream connections.  There will be no more data written to
    // this output after this isFlushing flag is set.
    //
    // Only source blocks are allowed to declared that they are
    // finished running the stream. ??
    //
    // See qsOutputDone().
    bool isFlushing;
};



struct QsStreamJob {

    // The simple block can have a stream job if there are connected
    // inputs or outputs.
    //
    // We get just one stream job per simple block, but only if
    // qsSetNumOutputs() or qsSetNumInputs() are called in the block's
    // declare() callback function; otherwise QsSimpleBlock::streamJob is
    // 0.
    //
    struct QsJob job; // inherit job


    // Stream I/O port numbers are indexes to these input and output
    // arrays:
    //
    //
    // Array of outputs.  Allocated.  Not 0 terminated.  Array length is
    // maxOutputs.  This is allocated in the block's declare() from a call
    // to qsSetNumOutputs().
    struct QsOutput *outputs;
    //
    // Array of inputs.  Allocated.  Not 0 terminated.  Array length is
    // maxInputs.
    //
    // This is allocated in the block's declare() from a call to
    // qsSetNumInputs().
    struct QsInput *inputs;


    // The number of stream ports in use when running to iterate through
    // for loops to check for queuing jobs in neighboring blocks.
    //
    // Both numInputs and numOutputs are calculated in qsGraph_start()
    // each time it is called.  They change because connections between
    // stream ports are added and removed.
    uint32_t numInputs;
    uint32_t numOutputs;

    // Connection port limits.  These 4 variables do not change after
    // the blocks declare() callback.
    uint32_t maxInputs;
    uint32_t maxOutputs;
    uint32_t minInputs;
    uint32_t minOutputs;


    /// Allocated Argument Arrays for stream flow() and flush() calls ///
    //
    // The number of inputs can change before start and after stop, and so
    // these flow() arguments may be reallocated at before stream flow
    // starts.
    //
    // Only the worker thread that is running this job may read of write
    // these arrays.
    //
    // The arguments to pass to the flow() call.  All are indexed by
    // input port number
    //
    // inputBuffers[] is an array of ring buffer read pointers.
    //
    void **inputBuffers; // allocated after start and freed after stop
    // inputLens[] values cannot exceed input maxRead
    size_t *inputLens;   // allocated after start and freed after stop
    //
    // advanceInputs is the length in bytes that the filter block advanced
    // the input buffers, indexed by input port number.
    size_t *advanceInputs; // allocated after start and freed after stop
    //
    // outputBuffers[] is an array of ring buffer write pointers.
    //
    void **outputBuffers; // allocated after start and freed after stop
    // outputLens[] cannot exceed output maxWrite.
    size_t *outputLens; // allocated after start and freed after stop
    //
    // advanceOutputs[i] is the amount output was advanced in flow() or
    // flush() call, for output channel i.  This cannot be larger than
    // the output maxWrite for that output port.
    size_t *advanceOutputs;

    // Since flow() is required, we can use it as a flag to show that the
    // first flow start sequence has been called; so that we can tell if
    // construct() and destroy() need to be called.
    //
    int (*flow)(const void * const in[], const size_t inLens[],
            uint32_t numIn, void * const out[], const size_t outLens[],
            uint32_t numOut, void *userData);
    // flush() is optional.  If present it is called when the stream is
    // ending and the stream is not obeying the flow() callback
    // requirements, like having enough input data that the block would
    // have liked.  If not present flow() is called in place of flow().
    int (*flush)(const void * const in[], const size_t inLens[],
            uint32_t numIn, void * const out[], const size_t outLens[],
            uint32_t numOut, void *userData);

    // If a block has no input it is a source.  We can find such blocks
    // from j->numInputs == 0.  isSource will not be set for this case.
    // The block being a source is just implied by j->numInputs == 0.
    //
    // A block may also be a source if it declares that it is (setting
    // isSource), even if it has or will have inputs, so that we may have
    // graphs with changes in flow patterns like blocks that do not
    // necessarily always output to their outputs.  Just because an input
    // port is connected in no way means that the block will always be
    // feed input; the flow into an input port could be conditional on
    // whatever logic that is present.
    //
    // This is one of the many ways that quickstream streams are not like
    // GNUradio streams.
    //
    bool isSource; // As declared by the block.

    // 
    // All the outputs of this stream job are finished being written to.
    // The output buffers may still need to be read (consumed) by the
    // connected inputs.
    //
    //bool isFlushing;

    // The block is finished getting flow() or flush() called for a given
    // stream start/stop cycle.
    //
    bool isFinished;

    // Set when calling flow() or flush().
    //
    bool busy;


    // If input or output advances for this stream job in the last flow()
    // or flush() call or a output port was flushed via qsOutputDone().
    //
    bool didIOAdvance;

    // We keep a sum of all inputLen[] and outputLens[] just after the
    // last flow() (or flush()) call.
    //
    size_t lastAvailableCount;


    // Used for the qsJob_lock() and qsJob_unlock(), and the accessing of
    // this structure.
    //
    // We use qsJob_addMutex() when we make stream I/O connections to
    // other blocks.
    pthread_mutex_t mutex;
};



extern
void DestroyStreamJob(struct QsSimpleBlock *b, struct QsStreamJob *sj);


extern
int qsBlock_connectStreams(struct QsSimpleBlock *inb, uint32_t inNum,
        struct QsSimpleBlock *outb, uint32_t outNum);

extern
int qsBlock_disconnectStreams(struct QsSimpleBlock *b,
        struct QsPort *port, uint32_t portNum);

extern
uint32_t HaltStreamBlocks(struct QsBlock *b);


// Return false if we can run the streams part of the graph.
//
// NOTE: The user must also check the return value of numInputs, and if it
// is 0 than there are no connections; so the connections may be good (no
// gaps) but there are none.
extern
bool CheckStreamConnections(struct QsGraph *g, uint32_t *numInputs);


static inline
struct QsStreamJob *
GetStreamJob(uint32_t inCallbacks, struct QsSimpleBlock **b_out) {

    struct QsSimpleBlock *b = GetBlock(inCallbacks,
            0, QsBlockType_simple);
    ASSERT(b, "Not a simple block in declare()");

    if(!b->streamJob) {
        b->streamJob = calloc(1, sizeof(*b->streamJob));
        ASSERT(b->streamJob, "calloc(1,%zu) failed",
                sizeof(*b->streamJob));
        CHECK(pthread_mutex_init(&b->streamJob->mutex, 0));

        // RANT:
        //
        // This thread pool "job" construct makes the stream code so much
        // better than the previous iterations of this code.  The job
        // provides a generic under layer construct for any
        // work()/inter-thread communication method; it's abstract.  Go
        // down the rabbit hole and see for yourself.  Finding this magic
        // coding "layer" required a month of research.  It's so wonky
        // that theirs no fucking way to publish it, except to just use it
        // in this code.
        //
        // The thread pool "job" abstraction is not seen by quickstream
        // users at any level: not block writers, application builders,
        // and certainly not graph builders or end application users.
        // It's strictly a, very important, libquickstream.so developer
        // abstraction; and it has lots of test programs in ../tests/
        // which made it possible.  Without testing this would be total
        // shit.
        //
        // Unless you go down the rabbit hole, you'll not understand or
        // appreciate this.
        //
        qsJob_init((void *) b->streamJob, (void *) b,
                (bool (*)(struct QsJob *j)) StreamWork,
                0/*0=>no destroy*/, 0/*peers=0 => not shared*/);
        qsJob_addMutex((void *) b->streamJob, &b->streamJob->mutex);
    }

    if(b_out)
        *b_out = b;


    return b->streamJob;
}


