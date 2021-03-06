#ifndef __qsblock_h__
#define __qsblock_h__



/** \page block quickstream interfaces for blocks

 */

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


// QsParameter is the parameter queue
struct QsParameter;
struct QsBlock;
struct QsGraph;


enum QsParameterType {

    QsNone = 0,
    QsDouble,
    QsUint64,
    QsString
};


/** Kind of parameter

 There are 3 kinds of parameters.
 */
enum QsParameterKind {

    QsAny = 0,  /** No particular kind of parameter */
    QsConstant, /** Gets pushed to setters, but not at flow-time */
    QsGetter,   /** Gets pushed to setters at flow-time */
    QsSetter    /** Is set from constant and getter */
};




/** The default input read promise
 */
#define QS_DEFAULTMAXREADPROMISE    ((size_t) 1024)


/** The default maximum length in bytes that may be written
 *
 * A filter block that has output may set the maximum length in bytes that
 * may be written for a given qsOutput() and qsGetBuffer() calls for a
 * given output port number.  If the value of the maximum length in bytes
 * that may be written was not set in the block start() function it's
 * value
 * will be QS_DEFAULTMAXWRITE.
 */
#define QS_DEFAULTMAXWRITE         ((size_t) 1024)


/** In general the idea of threshold is related to input triggering.
 * In the simplest case we can set a input channel threshold.
 */
#define QS_DEFAULTTHRESHOLD        ((size_t) 1)


/** bit flags for creating parameters */

/** With \p QS_SETS_WHILE_PAUSED set in qsParameterSetterCreate() a
 setter parameter will be it's callback may be called before start. */
#define QS_SETS_WHILE_PAUSED (01)



/** \section quickstream parameters
 

 These are the only possible connections pairs.  Though very limited,
 can effectively make very complex parameter passing topologies.


     Connection types              action
    ----------------------  ------------------------------------------

    Getter    --> Setter    values pushed repeatedly after graph start

    Constant <--> Constant  values set to all while graph is paused
                            and before start

    Constant  --> Setter    OPTIONAL, setCallback() is called before
                            start (at pause) each time the connection
                            group value changes



    Getter is always a source point.
    Setter is always a sink point.
    Setter is only connected to once or none.


    The Getter's value cannot be set from outside the block code.  We
    think of it as a way for a block to publish values as it runs.

    The is there is a value in Setter parameter's connected group of
    parameters and QS_SETS_WHILE_PAUSED is set, the Setter's setCallback()
    will be called just before the block's start().  After the first time
    the Setter's setCallback() is called it will not be called again
    until there is a value pushed from the connected parameters or it is
    reconnected to a new group with a value to push.

    Constant parameter has intrinsic value state, which can only be
    changed when the graph is paused.  The value when changed is
    immediately pushed to all parameters that are connected.  The topology
    of the Constant connections is always a fully connected topological
    graph for all Constants and Setters connected to the group.  At the
    time of a connection the parameter that is the "from" argument will
    have the priority for being the value, if it has a value yet.

    Implementing: Constant --> Setter causes the setters setCallback() to
    be called after every time the value is set but only if the graph is
    not paused if the setters QS_SETS_WHILE_PAUSED flag is not set.

    The benefit of having the Getter, Setter, and Constant be a super
    class of parameter can make it so that we can more easily compare
    and share values between these objects, automatically finding data
    value size and type compatibilities.

    This parameter idea screams for an interface with C++ template
    functions.  C++ template functions could be a little more efficient
    than the C functions.  One could consider making the C++ version the
    native implementation instead of making C++ wrappers of the C
    functions.

    If one wishes to change a parameter continuously, then said parameter
    is no longer a parameter and it should be implemented as a series of
    values in a quickstream stream.
*/


#ifdef BUILD_LIB
#define EXPORT   __attribute__((visibility("default")))
#else
#define EXPORT  /*empty macro*/
#endif



/** add a dynamic shared object (DSO) file to be loaded when running the
 block

 Adds an addition DSO plugin file and is used at run/flow time.

 Whither or not adding the run/flow time file succeeds will not be known
 when this function returns, we only determine if the file exists and is
 executable.  The file is not loaded until the flow graph first runs.

 This function can only be called in the plugin's declare() function.  The
 added run/flow time plugin file will contain all other quickstream plugin
 functions.

 The search paths used to find the file are the same as those that where
 used to load the DSO that has the function that called this function.

 The point of this is to give the block developer a method to not load
 libraries that are not needed until the flow graph runs.  It will
 decrease the run-time object bloat of the quickstream builder program
 which only needs to call the block's declare() functions.  The number
 of unused libraries could get larger for each block that it connects,
 where not for this method.

 quickstream does not use another programming language like yml or XML to
 define declarative block state.  The blocks are fully defined in the
 programming language that the blocks are written in.  This simplifies the
 internal structure of quickstream, compared to adding a parser and
 another language to facilitate defining generic declarative block state.
 Also this lets you write a complete fully working block with one C/C++
 source file, and no other files, not 30, like in GNUradio.  I really don't
 know how many files it takes to make a GNUradio block, I gave up on it at
 some point.  It is just too fucking stupid.  At this time GNUradio has a
 generic block generator program that is broken.  Hence, the tutorial on
 building a GNUradio block is broken and I've given up on fixing bugs in
 GNUradio, the code writers are fucking stupid-heads.  They do not know
 what robust code is.  They have made the process of making a block so
 complex that I find it easier to write my own streaming API than write a
 GNUradio block.

 This method breaks the rules of good programming practice, but without
 it, or something like it, the complexity of quickstream increases
 tremendously.  Common practice would be to add another language to the
 mix, forcing users to use two or more languages just to get the hello
 world example of usage.  We just stick with simple design.  Simple is
 good.  We argue that it's easier to add other languages, be they
 declarative or imperative, after you build a complete set of
 functionality.  It's not easy to build an imperative interface from a
 declarative one (pretty much impossible); it's easier to build a
 declarative interface from an imperative one (if not trivial, it's at
 least straight forward).

 The loading of the additional DSO will not happen until later when the
 program is flowing/running.  Therefore, the failure of this action will
 not be determined until long after this function returns.  This function
 merely just records the \p filename and \p userData so that the block may
 load the DSO later.

*/
EXPORT
extern
void qsAddRunFile(const char *filename, void *userData);

/* Get the pointer that was passed to qsAddRunFile()

 Since quickstream separates the all global data between running DSOs,
 we needed a way to share data between to DSO's that are in the same block.

 This function is only valid in a block callback function that is not
 declare().

 \return a pointer that user passed to qsAddRunFile() which was called in the
 blocks declare() function.
*/
EXPORT
extern
void *qsRunFileData(void);


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

 \param setCallback user callback that is called any time the parameter is set
 by a 
 
 \param userData

 \param flags bit mask of flags.  Setting QS_SETS_WHILE_PAUSED makes it so
 that \p setCallback can be called before flow start and it may be
 connected to a constant parameter.

 \return a setter parameter pointer on success, and 0 is returned if
 the parameter already exists, or other error.
 */
EXPORT
extern
struct QsParameter *
qsParameterSetterCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData),
        void *userData, const void *initialValue);



/** create an changing output parameter, or getter parameter

 /param initialValue if set will be the first value sent to all connected
 setter parameters.

 */
EXPORT
extern
struct QsParameter *
qsParameterGetterCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize,
        const void *initialValue);


/** check if a getter parameter is connected

 Getter parameters feed setter parameters in other blocks when they are
 connected.  If a getter parameter is not connected then the getter is
 effectively neutralised, and has not effect when the stream is running.

 /param getter is a pointer to a getter parameter that we are checking to
 see if it is connected to a setter parameter in another block.

 /return the number of times the getter is connected to a setter.
 */
EXPORT
extern
uint32_t
qsParameterNumConnections(struct QsParameter *getter);


/** create an constant output parameter

 Constant parameters are pushed to the any number of connected setter
 parameters just before each stream graph start.  Constant parameters may
 also be connected any number of other constant parameters.


 \param setCallback if set, is called any time there is a different value
 set.  Since constant parameter do not change while the stream is running
 when \p setCallback is called it is called in the list and thread with
 all the blocks that are listening to this parameter channel, as apposed
 to setter parameters, which get their \p setCallbacks called while the
 stream is running/flowing.

 As an alternative to using the \p setCallback a quickstream API user can
 poll the current value with qsParameterGetValue().

 */
EXPORT
extern
struct QsParameter *
qsParameterConstantCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData),
        void *userData, const void *initialVal);

EXPORT
extern
void qsParameterSetCallback(struct QsParameter *p,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData));


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
EXPORT
extern
uint32_t qsParameterGetterPush(struct QsParameter *getter,
        const void *value);


/** Get the type of a parameter object

 \param p is a pointer to the parameter object.

 \return the type of a parameter object.
 
 */
EXPORT
extern
enum QsParameterType qsParameterGetType(const struct QsParameter *p);


/** Get the last parameter value that was pushed or set

 Calling qsParameterGetValue() does not effect when and if a setter parameter's
 setCallback is called, this just poll the last queued setter parameter value.

 \param p a pointer to a parameter.

 \param value points to memory that the caller manages.  The memory must be of
 size that is the size of the parameter.

 /param p must be a getter, setter or constant parameter.
*/
EXPORT
extern void
qsParameterGetValue(struct QsParameter *p, void *value);


/** Get the value entry size of a parameter object

 \param p is a pointer to the parameter object.

 \return the value entry size of a parameter object.

 */
EXPORT
extern
size_t qsParameterGetSize(const struct QsParameter *p);


/** get a block name from the block pointer

 \param block is a pointer to the block.  If block is 0
 the current running block is used.

 \return a pointer to the block name string.
 The returned memory is managed by the block object.
 Do not write to it.
 */
EXPORT
extern
const char *qsBlockGetName(const struct QsBlock *block);


/** Get a pointer to a shared parameter object

 \param block the block which owns the parameter we are seeking.
 See qsBlockGetName().  If \p block is 0 the current running block
 will be used.

 \param pname is the name of the parameter, which is unique for a given
 block.

 \param isSetter is true than we are looking for a setter parameter, else
 we are not looking for a setter parameter.  A getter parameter and a
 setter parameter may have the same name.  But a getter and constant
 parameter may not have the same name.

 \return a pointer to the parameter object, or 0 if not found.
 */
EXPORT
extern
struct QsParameter *qsParameterGetPointer(struct QsBlock *block,
        const char *pname, enum QsParameterKind kind);


/** Get kind of parameter
 */
EXPORT
extern
enum QsParameterKind qsParameterGetKind(const struct QsParameter *p);



EXPORT
extern
void
qsParameterGetValueByName(const char *pname, void *value, size_t size);



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
        BLOCK_declare.so which is a DSO (dynamic shared object) that
        contains the declare() function, where BLOCK is the plugin file
        name that contains the modules flow-time functions.  We use the
        word declare as it refers to a process of constructing without
        external input, it declares the block as it is seen from outside
        the block.  The benefit of separating the plugin module into two
        DSO files is that the declare file may not need to link to any
        libraries that may be needed for the module to run at
        run/flow-time.  To be clear, we mean that declare does not
        configure, and defines the structure of the block in terms of just
        the service quickbuild *block* API and optional quickstream
        *build* API which can load other blocks into super-blocks.

        For super blocks that create blocks the declare() function may
        load other module blocks, and call their declare() functions.

    - **run** we link and load more code and can run the flow graph.  In
        this mode declare() is called before construct().   If there is
        a declare DSO file, then the declare() function in it must
        be the same as the declare() in the run/flow-time plugin DSO
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
EXPORT
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
EXPORT
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
EXPORT
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
EXPORT
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
EXPORT
extern
void qsSetInputReadPromise(uint32_t inputPortNum, size_t len);


/** Create an output buffer that is associated with the listed ports

 qsSetOutputMaxWrite() can only be called in the block's start() function.
 If qsSetOutputMaxWrite() is not called for a given port number an output
 buffer will be created with the maxWrite parameter set to the default
 value of QS_DEFAULTMAXWRITE.
 
 The total amount of memory allocated for this ring buffer depends on
 maxWrite, and other parameters set by other blocks that may be
 accessing this buffer down stream and up stream.

 \param outputPortNum the output port number that the block will use to
 write to this buffer via qsGetOutputBuffer() and qsOutput().

 \param maxWriteLen this block promises to write at most maxWriteLen
 bytes to this output port.  If the block writes more than that memory
 may be corrupted.

 \memberof CBlockAPI
 */
EXPORT
extern
void qsSetOutputMaxWrite(uint32_t outputPortNum, size_t maxWriteLen);

/**
 If qsSetOutputMaxWrite() is not called in the block's start() function than
 the maximum output write length may be set by the block user.

 \param outputPortNum the output port number that the block will use to
 write to this buffer via qsGetOutputBuffer() and qsOutput().

 \return the maximum output write length for the given port number, or 0 if
 there is no output port with that port number.
 */
EXPORT
extern
size_t qsGetOutputMaxWrite(uint32_t outputPortNum);


/** pair input and output ports as to use "pass-through" buffer

 A pass-through buffer shared the memory mapping between the input port
 to an output port.  The block reading the input can write it's output
 to the same virtual address as the input.  The input is overwritten by
 the output.  The size of the input data must be the same as the output
 data, given they are the same memory.  We are just changing the value
 in the input memory and passing it through to the output; whereby
 saving the need for transferring data between two separate memory
 buffers.

 This only declares that if the given ports are used that they will be
 using a sharing a pass-through buffer.  If the ports are not connected
 than the pass-through buffer will not exist at stream run/flow time.

 This can only be called in the block's declare() function.

 \param inputPortNum the input port number that will share memory with
 the output.

 \param outputPortNum the output port number that will share memory with
 the input.

 \return 0 on success.  If the buffer that corresponds with the output port
 is already passed through to this or another input to any block this
 with fail and return non-zero.

 \memberof CBlockAPI
 */
EXPORT
extern
int qsPassThroughBuffer(uint32_t inputPortNum, uint32_t outputPortNum);


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
EXPORT
extern
const char* qsBlockGetName(const struct QsBlock *block);


/** Get a block pointer from the blocks loaded name

 \param graph is the graph that the block was loaded with.

 \param bName is the name of the block.

 \return a pointer to the struct QsBlock object, or 0 if the
 block with that name was not found in that graph.
 */
EXPORT
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
EXPORT
extern
int qsTriggerSignalCreate(int signum,
        int (*triggerCallback)(void *userData),
        void *userData);



EXPORT
extern
void qsBlockSetNumInputs(uint32_t min, uint32_t max);


EXPORT
extern
void qsBlockSetNumOutputs(uint32_t min, uint32_t max);


EXPORT
extern
void qsBlockSetNumInputsEqualsNumOutputs(bool inputsEqualsOutputs);


/** Get the name of a parameter object

 \param p is a pointer to the parameter object.

 \return the value entry size of a parameter object.

 */
EXPORT
extern
const char *qsParameterGetName(const struct QsParameter *p);


/** The block plugin declare module callback function

 The block plugin module declare callback, \p declare(), is the only
 required block plugin callback function.  \p declare() can only call
 *builder* and *block* functions, or so called block declare functions.
 It should not access hardware devices, or initialize anything, for that
 would make the quickstream *builder* process use more resources than
 necessary.

 The declare() function may setup interfaces to it's block.

 declare() is called when qsGraphLoadBlock() is called.

 \param graph is a pointer to the graph that was used to load this block
 module.

 \return 0 on success, greater than 0 in the case where the DSO (dynamic
 shared object) file should be unloaded, and less than 0 if there is a
 error that we can't recover from.

 \memberof CBlockAPI
 */
int declare(void);



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
 function: qsSetOutputMaxWrite(), qsPassThroughBuffer(),
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
 function: qsSetOutputMaxWrite(), qsPassThroughBuffer(),
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


#ifdef __cplusplus
}
#endif


#undef EXPORT


#endif // #ifndef __qsblock_h__
