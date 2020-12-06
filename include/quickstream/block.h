#ifndef __qsblock_h__
#define __qsblock_h__



/** \page block quickstream interfaces for blocks

 */

#include <stdlib.h>
#include <inttypes.h>


// QsParameter is the parameter queue
struct QsParameter;
struct QsBlock;
struct QsGraph;


enum QsParameterType {

    QS_NONE = 0,
    QS_DOUBLE,
    QS_FLOAT,
    QS_UINT64
};


/** The default input read promise
 */
#define QS_DEFAULTMAXREADPROMISE    ((size_t) 1024)


/** The default maximum length in bytes that may be written
 *
 * A filter block that has output may set the maximum length in bytes that
 * may be written for a given qsOutput() and qsGetBuffer() calls for a
 * given output port number.  If the value of the maximum length in bytes
 * that may be written was not set in the filter start() function it's
 * value
 * will be QS_DEFAULTMAXWRITE.
 */
#define QS_DEFAULTMAXWRITE         ((size_t) 1024)


/** In general the idea of threshold is related to input triggering.
 * In the simplest case we can set a input channel threshold.
 */
#define QS_DEFAULTTHRESHOLD        ((size_t) 1)


/** bit flag to mark keeping parameter get callback across restarts. */
#define QS_KEEP_AT_RESTART (02)
/** free get callback user data */
#define QS_FREE_USERDATA   (04)
/** queue up callback before flow() */
#define QS_Q_BEFORE_FLOW   (010)
/** queue up callback after flow() */
#define QS_Q_AFTER_FLOW    (020)



/** create an input parameter, or setter parameter

 Create an input parameter.  Input parameters are values that can be set
 at flow/run time.  It's up to the block to do what ever it wants with
 the value that is passed to it via a callback function.  The block does
 not necessarily know where the value is coming from.  Parameter
 connections are setup at a flow graph builder level and so the block
 that creates this input parameter does not know where the values being
 set comes from.

 \param block is the block that the setter parameter is being built for.
 If block is 0 the current block that is having a plugin callback
 function called is used.

 \param pname is the name of the setter parameter.  The name of the
 setter parameter must be unique for the block.

 \param type is the type of the parameter.

 \param psize is the size in bytes of the values set in this parameter.
 If this value is an array the \p psize is the total size in bytes of
 the array.

 \param setCallback is the callback function that will be called before
 or after the block flow() function, after each time a connected getter
 parameter changes.

 The \p setCallback function may call qsParameterGetterPush().

 The \p setCallback function should avoid making long time blocking calls.
 The intent of this parameter mechanism is to just think of it a way to
 quickly pass small chunks of data asynchronously.  The setCallback()
 function should just qsParameterGetterPush() to any getter parameters
 that may be related, copy the data in what ever form the block needs, and
 return.

 \return a setter parameter pointer on success, and 0 is returned if
 the parameter already exists and the QS_GET flag was not used, or the
 \p psize and \p type do not match the existing parameter.

 */
extern
struct QsParameter *
qsParameterSetterCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize,
        void (*setCallback)(struct QsParameter *p,
            const void *value, size_t size, void *userData),
        void (*cleanup)(struct QsParameter *, void *userData),
        void *userData, uint32_t flags);




/** create an changing output parameter, or getter parameter

 */
extern
struct QsParameter *
qsParameterGetterCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize);



/** check if a getter parameter is connected

 Getter parameters feed setter parameters in other blocks when they are
 connected.  If a getter parameter is not connected then the getter is
 effectively neutralised, and has not effect when the stream is running.

 /param getter is a pointer to a getter parameter that we are checking to
 see if it is connected to a setter parameter in another block.

 /return the number of times the getter is connected to a setter.
 */
extern
uint32_t
qsParameterNumConnections(struct QsParameter *getter);


/** create an constant output parameter

 Constant parameters are pushed to the any number of connected setter
 parameters just before each stream graph start.  Constant parameters may
 also be connected any number of other constant parameters.

 */
extern
struct QsParameter *
qsParameterConstantCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize, void *initialVal);




/** Push a getter parameter to all setter parameters that are connected

 Push a value to all connected blocks setter parameters.  Because this is
 running in the thread of the block that owns this getter parameter the
 needed setter callback functions will be queue up, and called before or
 after the parameter setter block's flow() function thread.

 \param parameter is a pointer to the getter parameter.

 This value pointer memory must be consistent with the getter parameter,
 in size and type.

 \param value is a pointer to the data that is to be copied to the
 connected setter parameters.  The memory pointed to by \p value must be
 the same size is the setter parameters that it is connected to.

 \return the number of setter parameters that the value was pushed to.  If
 the returned value was 0 than there are no connections and there will be
 no need to call this until the next stream flow restart.

 */
extern
uint32_t qsParameterGetterPush(struct QsParameter *getter,
        const void *value);



/*

 TODO: add this to the docs:

 There are two modes in which quickstream runtime runs:

    - **builder** we are getting description of the block and its'
        capabilities: all its' getter and setter parameters, its' optional
        fixed parameters with their default values, and it's limits on
        connecting input ports and output ports.  The block's internal
        structure provides this information to the builder through the
        API.

        A quickstream builder program may come in many forms, for example
        a GUI (graphical user interface) program or a command-line
        program.  The quickstream builder program may or may not be the
        same program the runs the flow in the stream graph.

        The block plugin module may provide a file named
        BLOCK_bootstrap.so which is a DSO (dynamic shared object) that
        contains the bootstrap() function, where BLOCK is the plugin file
        name that contains the modules flow-time functions.  We use the
        word bootstrap as it refers to a process of constructing without
        external input.  https://en.wikipedia.org/wiki/Bootstrapping The
        benefit of separating the plugin module into two DSO files is
        that the bootstrap file may not need to link to any libraries that
        may be needed for the module to run at run/flow-time.  To be
        clear, we mean that bootstrap does not configure, and defines the
        structure of the block in terms of just the q service uickbuild
        *block* API and optional quickstream *build* API which can load
        other blocks into super-blocks.

        For super blocks that create blocks the bootstrap() function may
        load other module blocks, and call their bootstrap() functions.

    - **run** we link and load more code and can run the flow graph.  In
        this mode getConfig() is called before construct().   If there is
        a getConfig DSO file, then the getConfig() function in it must
        be the same as the getConfig() in the run/flow-time plugin DSO
        file.  The optional file, named BLOCK_config.so, if it
        exists, is ignored in this **run** mode.
*/



/** Get an output buffer pointer

 qsGetBuffer() may only be called in the block's flow() function.  If a
 given block at a given input() call will generate output than
 qsGetBuffer() must be called and later followed by a call to
 qsOutput().

 If output is to be generated for this output port number than
 qsOutput() must be called some time after this call to qsGetBuffer().

 If qsGetBuffer() is not called in a given block flow() callback
 function there will be no output from the block in the given flow()
 call.

 \param outputPortNum the associated output port.  If the output
 port is sharing a buffer between other output ports from this
 block then the value of outputPortNum may be any of the output
 port numbers that are in the output port shared buffer group.

 \return a pointer to the first writable byte in the buffer.

 \memberof CBlockAPI
 */
extern
void *qsGetOutputBuffer(uint32_t outputPortNum);


/** advance data to the output buffers

 This set the current buffer write offset by advancing it \p len bytes.

 qsGetOutputBuffer() should be called before qsOutput().

 qsOutput() must be called in a block module input() function in order
 to have output sent to another block module.

 \param outputPortNum the output port number.

 \param len length in bytes to advance the output buffer.

 \memberof CBlockAPI
 */
extern
void qsOutput(uint32_t outputPortNum, size_t len);


/** advance the current input buffer

 qsAdvanceInput() can only be called in a block flow() function.

 In order to advance the input buffer a length that is not the length
 that was passed into the input() call, this qsAdvanceInput() function
 must be called in the input() function.  If qsAdvanceInput() is not
 called in input() than the input buffer will not be automatically be
 advanced, and an under-run error can occur is the read promise for that
 port is not fulfilled.  See qsSetInputReadPromise().

 This has no effect on output from the current block.  This only
 effects the current input port number that passed to input();

 \param inputPortNum the input port number that corresponds with the
 input buffer pointer that you wish to mark as read.

 \param len advance the current input buffer length in bytes.  \p len
 can be less than or equal to the corresponding length in bytes that was
 passed to the input() call.  len greater than the input length will be
 clipped to the length that was passed to input().

 \memberof CBlockAPI
 */
extern
void qsAdvanceInput(uint32_t inputPortNum, size_t len);


/** set the current block's input threshold

 Set the minimum input needed in order for current blocks flow()
 function to be called.  This threshold, if reached for the
 corresponding input port number, will cause input() to be called.  If
 this simple threshold condition is not adequate the blocks flow()
 function may quickly just return 0, and effectively wait for more a
 complex threshold condition to be reached by continuing to just quickly
 return 0 until the block sees the level of inputs that it likes.

 qsSetInputThreshold() may only be called in the block's start()
 function.

 \param inputPortNum the input port number that corresponds with the
 input buffer pointer that you wish to set a simple threshold for.

 \param len the length in bytes of this simple threshold.

 \memberof CBlockAPI
 */
extern
void qsSetInputThreshold(uint32_t inputPortNum, size_t len);


// Sets maxRead
/** Set the input read promise

 The block flow() promises to read at least one byte of data on a
 given input() call, if there is len bytes of input on the port
 inputPortNum to read.  If this promise is not kept the program will
 fail.  This is to keep the fixed ring buffers from being overrun.

 \param inputPortNum the input port number that the promise is made for.

 \param len length in bytes that the current block promises to read.

 The default len value is QS_DEFAULTMAXREADPROMISE.  If this default
 value is not large enough than you must call this.

 \memberof CBlockAPI
 */
extern
void qsSetInputReadPromise(uint32_t inputPortNum, size_t len);


/** Create an output buffer that is associated with the listed ports

 qsOutputBufferCreate() can only be called in the block's start()
 function.  If qsOutputBufferCreate() is not called for a given port
 number an output buffer will be created with the maxWrite parameter set
 to the default value of QS_DEFAULTMAXWRITE.
 
 The total amount of memory allocated for this ring buffer depends on
 maxWrite, and other parameters set by other blocks that may be
 accessing this buffer down stream.

 \param outputPortNum the output port number that the block will use to
 write to this buffer via qsGetOutputBuffer() and qsOutput().

 \param maxWriteLen this block promises to write at most maxWriteLen
 bytes to this output port.  If the block writes more than that memory
 may be corrupted.

 \memberof CBlockAPI
 */
extern
void qsCreateOutputBuffer(uint32_t outputPortNum, size_t maxWriteLen);

/** create a "pass-through" buffer

 A pass-through buffer shared the memory mapping between the input port
 to an output port.  The block reading the input can write it's output
 to the same virtual address as the input.  The input is overwritten by
 the output.  The size of the input data must be the same as the output
 data, given they are the same memory.  We are just changing the value
 in the input memory and passing it through to the output; whereby
 saving the need for transferring data between two separate memory
 buffers.

 \param inputPortNum the input port number that will share memory with
 the output.

 \param outputPortNum the output port number that will share memory with
 the input.

 \param maxWriteLen is the maximum length in bytes that the calling
 block promises not to exceed calling qsOutput().

 \return 0 on success.  If the buffer that corresponds with the output port
 is already passed through to this or another input to any block this
 with fail and return non-zero.

 \memberof CBlockAPI
 */
extern
int qsCreatePassThroughBuffer(uint32_t inputPortNum, uint32_t outputPortNum,
        size_t maxWriteLen);


/** Get the name of the block

 The name of a block is set by the person running the quickstream
 program.  The name is unique for that loaded block running in a given
 program.  The name is generated automatically of can be set using the
 qsGraphLoadBlock() function in a quickstream runner program.

 qsGetBlockName() can only be called in a block module in it's
 construct(), start(), stop(), and destroy() functions.

 If you need the block name in flow() get it in start() or
 construct().  The blocks name will never change after it's loaded.

 \return a string that you should only read.

 \memberof CBlockAPI
 */
extern
const char* qsGetBlockName(void);


/** Get a block pointer from the blocks loaded name

 \param graph is the graph that the block was loaded with.

 \param bName is the name of the block.

 \return a pointer to the struct QsBlock object, or 0 if the
 block with that name was not found in that graph.
 */

extern
struct QsBlock *qsBlockGetFromName(struct QsGraph *graph,
        const char *bName);


/** Create a trigger callback from a operating system signal

 How blocks code is run is very restricted and controlled by quickstream.
 Blocks do not control how and what threads run their functions.  Hence
 quickstream can't let blocks catch signals; doing so could corrupt
 memory in the running program.

 qsTriggerSignalCreate() sets up a signal catcher that will eventually
 lead to calling the \p triggerCallback function which the user passes.
 The calling thread will be assigned for a quickstream thread from the
 block's assigned thread-pool.  The how threads run block's functions is
 configured from code not in the blocks, from programs that run the stream
 graph.

 \param signum is the system signal number.  See "kill -L".

 \verbinclude triggerCallback.dox

 \param userData is passed to triggerCallback() function each time it is
 called.

 \return 0 on success.

 \memberof CBlockAPI
 */
extern
int qsTriggerSignalCreate(int signum,
        int (*triggerCallback)(void *userData),
        void *userData);


/** The block plugin bootstrap module callback function

 The block plugin module bootstrap callback, \p bootstrap(), is the only
 required block plugin callback function.  \p bootstrap() can only call
 *builder* and *block* functions, or so called block bootstrap functions.
 It should not access hardware devices, or initialize anything, for that
 would make the quickstream *builder* process use more resources than
 necessary.

 The bootstrap() function may setup options in it's block.

 bootstrap() is called when qsGraphLoadBlock() is called.

 \param graph is a pointer to the graph that was used to load this block
 module.

 \return 0 on success, greater than 0 in the case where the DSO (dynamic
 shared object) file should be unloaded, and less than 0 if there is a
 error that we can't recover from.

 \memberof CBlockAPI
 */
int bootstrap(void);



/** optional construct() function

 Triggers setup in construct() do not need to be setup again at each
 stream flow cycle.

 /return 0 on success, and 1 to unload the block, and less than 0 on
 error.

 */
int construct(void);

/** optional destructor function

 This function, if present, is called only once just before the block
 is unloaded.

 \memberof CBlockAPI
 */
void destroy(void);


/** optional block start function

 Triggers setup in start() do need to be setup again at each
 stream flow cycle in start() in the next flow cycle.


 This function, if present, is called each time the stream starts
 running, just before any block in the graph has it's \ref flow()
 function called.  We call this time the start of a flow cycle.  After
 this is called \ref flow() will be called in a regular fashion for
 the duration of the flow cycle.
 
 This function lets that block determine what the number of inputs and
 outputs before it has it's \ref input() function called.  For "smarter"
 blocks this can spawn a series of initialization interactions between
 the "smarter" blocks in the stream graph.

 The following functions may only be called in the block's start()
 function: qsCreateOutputBuffer(), qsCreatePassThroughBuffer(),
 qsSetInputThreshold(), and qsSetInputReadPromise().
 *
 \param numInPorts is the number of input buffers in the inBuffers input
 array.  numInPorts will be the same value for the duration of the
 current flow cycle.

 \param numOutPorts is the number of output buffers that this block is
 writing to.  numOutPorts will be the same value for the duration of the
 current flow cycle.

 \return 0 on success, less than 0 on failure to signal the program to
 fail, and greater than 0 to remove the start() callback on all future
 starts for the current program.
 
 \memberof CBlockAPI
 */
int start(uint32_t numInPorts, uint32_t numOutPorts);


/** Optional block stop function

 This function, if present, is called each time the stream stops
 running, just after any block in the stream has it's \ref input()
 function called for the last time in a flow/run cycle.

 \param numInPorts is the number of input buffers was in inBuffers input
 array.  numInPorts will be the same value as 173it was when the
 corresponding start() and input() was called.

 \param numOutPorts is the number of output buffers that this block is
 writing to.  numOutPorts will be the same value as it was when the
 corresponding start() and input() was called.

 \return 0 on success, or non-zero on failure.

 \memberof CBlockAPI
 */
int stop(uint32_t numInPorts, uint32_t numOutPorts);

/** Optional block start function

 This function, if present, is called each time the stream starts
 running, just before any block in the stream has it's \ref input()
 function called.  We call this time the start of a flow cycle.  After
 this is called \ref input() will be called in a regular fashion for
 the duration of the flow cycle.

 This function lets that block determine what the number of inputs and
 outputs before it has it's \ref input() function called.  For "smarter"
 blocks this can spawn a series of initialization interactions between
 the "smarter" blocks in the stream.

 The following functions may only be called in the block's start()
 function: qsCreateOutputBuffer(), qsCreatePassThroughBuffer(),
 qsSetInputThreshold(), qsSetInputReadPromise(), and
 qsGetBlockName().

 \param numInPorts is the number of input buffers in the inBuffers input
 array.  numInPorts will be the same value for the duration of the
 current flow cycle.

 \param numOutPorts is the number of output buffers that this block is
 writing to.  numOutPorts will be the same value for the duration of the
 current flow cycle.

 \return 0 on success, or non-zero on failure.

 \memberof CBlockAPI
 */
int start(uint32_t numInPorts, uint32_t numOutPorts);


/** Optional block stop function

 This function, if present, is called each time the stream stops
 running, just after any block in the stream has it's \ref input()
 function called for the last time in a flow/run cycle.

 \param numInPorts is the number of input buffers was in inBuffers input
 array.  numInPorts will be the same value as 173it was when the
 corresponding start() and input() was called.

 \param numOutPorts is the number of output buffers that this block is
 writing to.  numOutPorts will be the same value as it was when the
 corresponding start() and input() was called.

 \return 0 on success, or non-zero on failure.

 \memberof CBlockAPI
 */
int stop(uint32_t numInPorts, uint32_t numOutPorts);



int flow(void *in[], const size_t lenIn[],
        uint32_t numIn, uint32_t numOut);


int flush(void *in[], const size_t len[],
        uint32_t numInputs, uint32_t numOutputs);




#endif // #ifndef __qsblock_h__
