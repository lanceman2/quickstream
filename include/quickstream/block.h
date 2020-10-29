#ifndef __qsblock_h__
#define __qsblock_h__



/** \page block quickstream interfaces for blocks

 */
 
#include <inttypes.h>


// QsParameter is the parameter queue
struct QsParameter;
struct QsBlock;


enum QsParameterType {

    QS_NONE = 0,
    QS_DOUBLE,
    QS_FLOAT,
    QS_UINT64
};



/** bit flag to mark keeping parameter get callback across restarts. */
#define QS_KEEP_AT_RESTART (02)
/** free get callback user data */
#define QS_FREE_USERDATA   (04)
/** queue up callback before work() */
#define QS_Q_BEFORE_WORK   (010)
/** queue up callback after work() */
#define QS_Q_AFTER_WORK    (020)



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
 or after the block work() function, after each time a connected getter
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


/** create an constant output parameter

 Constant parameters are pushed to the any number of connected setter
 parameters just before each stream graph start.  Constant parameters may
 also be connected any number of other constant parameters.

 */
extern
struct QsParameter *
qsParameterConstantCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize);




/** Push a getter parameter to all setter parameters that are connected

 Push a value to all connected blocks setter parameters.  Because this is
 running in the thread of the block that owns this getter parameter the
 needed setter callback functions will be queue up, and called before or
 after the parameter setter block's work() function thread.

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
uint32_t qsParameterGetterPush(struct QsParameter *getter, const void *value);



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


/** The block plugin bootstrap module callback function

 The block plugin module bootstrap callback, \p bootstrap(), is the only
 required block plugin callback function.  \p bootstrap() can only call
 *builder* and *block* functions, or so called block bootstrap functions.
 It should not access hardware devices, or initialize anything, for that
 would make the quickstream *builder* process use more resources than
 necessary.

 The bootstrap() function may setup options in it's block.

 \return 0 on success, greater than 0 in the case where the DSO (dynamic
 shared object) file should be unloaded, and less than 0 if there is a
 error that we can't recover from.

 \memberof CBlockAPI

 */
int bootstrap(void);



/** optional construct() function


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
 
 This function, if present, is called each time the stream starts
 running, just before any block in the graph has it's \ref work()
 function called.  We call this time the start of a flow cycle.  After
 this is called \ref work() will be called in a regular fashion for
 the duration of the flow cycle.
 
 This function lets that block determine what the number of inputs and
 outputs before it has it's \ref input() function called.  For "smarter"
 blocks this can spawn a series of initialization interactions between
 the "smarter" blocks in the stream graph.

 The following functions may only be called in the block's start()
 function: qsCreateOutputBuffer(), qsCreatePassThroughBuffer(),
 qsSetInputThreshold(), qsSetInputReadPromise(), and
 qsBlockGetName().
 *
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


/** Optional filter stop function
 *
 * This function, if present, is called each time the stream stops
 * running, just after any filter in the stream has it's \ref input()
 * function called for the last time in a flow/run cycle.
 *
 * \param numInPorts is the number of input buffers was in inBuffers input
 * array.  numInPorts will be the same value as 173it was when the
 * corresponding start() and input() was called.
 *
 * \param numOutPorts is the number of output buffers that this filter is
 * writing to.  numOutPorts will be the same value as it was when the
 * corresponding start() and input() was called.
 *
 * \return 0 on success, or non-zero on failure.
 *
 * \memberof CBlockAPI
 */
int stop(uint32_t numInPorts, uint32_t numOutPorts);

/** optional filter start function
 *
 * This function, if present, is called each time the stream starts
 * running, just before any filter in the stream has it's \ref input()
 * function called.  We call this time the start of a flow cycle.  After
 * this is called \ref input() will be called in a regular fashion for
 * the duration of the flow cycle.
 *
 * This function lets that filter determine what the number of inputs and
 * outputs before it has it's \ref input() function called.  For "smarter"
 * filters this can spawn a series of initialization interactions between
 * the "smarter" filters in the stream.
 *
 * The following functions may only be called in the filters start()
 * function: qsCreateOutputBuffer(), qsCreatePassThroughBuffer(),
 * qsSetInputThreshold(), qsSetInputReadPromise(), and
 * qsGetFilterName().
 *
 * \param numInPorts is the number of input buffers in the inBuffers input
 * array.  numInPorts will be the same value for the duration of the
 * current flow cycle.
 *
 * \param numOutPorts is the number of output buffers that this filter is
 * writing to.  numOutPorts will be the same value for the duration of the
 * current flow cycle.
 *
 * \return 0 on success, or non-zero on failure.
 *
 * \memberof CFilterAPI
 */
int start(uint32_t numInPorts, uint32_t numOutPorts);


/** Optional filter stop function
 *
 * This function, if present, is called each time the stream stops
 * running, just after any filter in the stream has it's \ref input()
 * function called for the last time in a flow/run cycle.
 *
 * \param numInPorts is the number of input buffers was in inBuffers input
 * array.  numInPorts will be the same value as 173it was when the
 * corresponding start() and input() was called.
 *
 * \param numOutPorts is the number of output buffers that this filter is
 * writing to.  numOutPorts will be the same value as it was when the
 * corresponding start() and input() was called.
 *
 * \return 0 on success, or non-zero on failure.
 *
 * \memberof CFilterAPI
 */
int stop(uint32_t numInPorts, uint32_t numOutPorts);



int work(void *in[], const size_t lenIn[],
        uint32_t numIn, uint32_t numOut);

int flush(void *buffer[], const size_t len[],
        uint32_t numInputs, uint32_t numOutputs);




#endif // #ifndef __qsblock_h__
