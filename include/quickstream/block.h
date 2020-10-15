#ifndef __qsblock_h__
#define __qsblock_h__



/** \page block quickstream interfaces for blocks
 *
 */
 
#include <inttypes.h>


// QsParameter is the parameter queue
struct QsParameter;
struct QsOptions;
struct QsBlock;


enum QsParameterType {

    QS_NONE = 0,
    QS_DOUBLE,
    QS_FLOAT,
    QS_UINT64
};



/** bit flag used to mark the use of regular expressions to find
 * parameter with qsParameterGet(), qsParameterForEach(), and
 * qsParameterDestroyForFilter().
 */
#define QS_PNAME_REGEX     (01)
/** bit flag to mark keeping parameter get callback across restarts. */
#define QS_KEEP_AT_RESTART (02)
/** free get callback user data */
#define QS_FREE_USERDATA   (04)
/** queue up callback before work() */
#define QS_Q_BEFORE_WORK   (010)
/** queue up callback after work() */
#define QS_Q_AFTER_WORK    (020)



/** get the app that the block was loaded with
 
 \param block the block

 \return a pointer to the app that loaded \p block.

 */



extern
struct QsBlock *qsBlockGetApp(struct QsBlock *block);



/** get a block pointer from the block name

 \param app the app that the block was loaded with.

 \param bname is the name of the block.

 \return a pointer to the block with the name.
 */
extern
struct QsBlock *qsBlockGetBlockByName(struct QsApp *app, const char *bname);


/** get a block name from the block pointer

 \param block is a pointer to the block.  If block is 0
 the current running block is used.

 \return a pointer to the block name string.
 The returned memory is managed by the block object.
 Do not write to it.
 */
extern
const char *qsBlockGetName(struct QsBlock *block);


/** get the number of input ports and output ports for a block

 This function is only valid after the stream has started and before the
 stream has stopped.

 \param block is a pointer to the block.

 \param numIn if non-zero is filled with the number of input ports that
 the block has.

 \param numOut if non-zero is filled with the number of output ports
 that the block has.

 \return 0 on success and -1 if the block was not found.
 */
extern
int qsBlockGetNumPorts(struct QsBlock *block,
        uint32_t *numIn, uint32_t *numOut);


/** Get a pointer to a shared parameter object

 \param block the block which owns the parameter we are seeking.
 See qsBlockGetName().  If \p block is 0 the current running block
 will be used.

 \param pname is the name of the parameter, which is unique for a given
 block.

 \param isGetter is true than we are looking for a getter parameter,
 else we are looking for a setter parameter.  A getter parameter and a
 setter parameter may have the same name.

 \return a pointer to the parameter object, or 0 if not found.

 */
extern
struct QsParameter *qsParameterGetPointer(struct QsBlock *block,
        const char *pname, bool isGetter);


/** Get the value entry size of a parameter object

 \param p is a pointer to the parameter object.

 \return the value entry size of a parameter object.

 */
extern
size_t qsParameterGetSize(struct QsParameter *p);


/** Get the name of a parameter object

 \param p is a pointer to the parameter object.

 \return the value entry size of a parameter object.

 */
extern
const char *qsParameterGetName(struct QsParameter *p);


/** Get the type of a parameter object

 \param p is a pointer to the parameter object.

 \return the type of a parameter object.
 
 */
extern
enum QsParameterType qsParameterGetType(struct QsParameter *p);


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


/** create an output parameter, or getter parameter

 */
extern
struct QsParameter *
qsParameterGetterCreate(struct QsBlock *block, const char *pname,
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


/** Iterate through the getter parameters via a callback function
 
 One of arguments \p app or \p block must be non-zero.

 \param app is ignored unless \p stream is 0.  If \p app is non-zero and
 \p stream is zero iterated through all selected parameters in all streams
 in  this \p app.

 \param block if not 0, restrict the range of the parameters to iterate
 through to just parameters in this block.

 \param pName if not 0, restrict the range of the parameters to iterate
 through to just parameters with this name.

 \param type if not 0, restrict the range of the parameters to iterate
 through to just parameters with this type.

 \param callback is the callback function that is called with each
 parameter and with all of the arguments set.  If \p callback() returns
 non-zero than the iteration will stop, and that will be the last time \p
 callback() is called for this call to \p qsParameterForEach().

 \param userData is user data that is passed to the callback every time it
 is called.

 \param flags if flags includes the bit QS_PNAME_REGEX \p pName will be
 interpreted as a POSIX Regular Expression and all parameters with a name
 matches the regular expression will have the get callback added to it.
 Use 0 otherwise.

 \return the number of parameters that have been iterated through; or the
 same as, the number of times \p callback() is called.

 */
size_t qsParameterGetterForEach(struct QsApp *app, struct QsBlock *block,
        const char *pName,
        enum QsParameterType type,
        int (*callback)(
            struct QsBlock *block, const char *pName,
            enum QsParameterType type, void *userData),
        void *userData, uint32_t flags);

/** Iterate through the getter parameters via a callback function
 
 \param app is ignored unless \p block is 0.  If \p app is non-zero and \p
 block is zero, all blocks are iterated through in all selected getter
 parameters in this \p app.

 \param block if block is not 0, restrict the range of the getter
 parameters to iterate through to just getter parameters owned by this
 block.

 One of arguments \p app or \p block must be non-zero.

 \param pName if not 0, restrict the range of the parameters to iterate
 through to just parameters with this name.

 \param type if not 0, restrict the range of the parameters to iterate
 through to just parameters with this type.

 \param callback is the callback function that is called with each
 parameter and with all of the arguments set.  If \p callback() returns
 non-zero than the iteration will stop, and that will be the last time \p
 callback() is called for this call to \p qsParameterForEach().

 \param userData is user data that is passed to the callback every time it
 is called.

 \param flags if flags includes the bit QS_PNAME_REGEX \p pName will be
 interpreted as a POSIX Regular Expression and all parameters with a name
 matches the regular expression will have the get callback added to it.
 Use 0 otherwise.

 \return the number of parameters that have been iterated through; or the
 same as, the number of times \p callback() is called.

 */
size_t qsParameterGetterForEach(struct QsApp *app, struct QsBlock *block,
        const char *pName,
        enum QsParameterType type,
        int (*callback)(
            struct QsBlock *block, const char *pName,
            enum QsParameterType type, void *userData),
        void *userData, uint32_t flags);


/** Iterate through the setter parameters via a callback function
 
 \param app is ignored unless \p block is 0.  If \p app is non-zero and \p
 block is zero, all blocks are iterated through in all selected setter
 parameters in this \p app.

 \param block if block is not 0, restrict the range of the getter
 parameters to iterate through to just setter parameters owned by this
 block.

 One of arguments \p app or \p block must be non-zero.

 \param pName if not 0, restrict the range of the parameters to iterate
 through to just parameters with this name.

 \param type if not 0, restrict the range of the parameters to iterate
 through to just parameters with this type.

 \param callback is the callback function that is called with each
 parameter and with all of the arguments set.  If \p callback() returns
 non-zero than the iteration will stop, and that will be the last time \p
 callback() is called for this call to \p qsParameterForEach().

 \param userData is user data that is passed to the callback every time it
 is called.

 \param flags if flags includes the bit QS_PNAME_REGEX \p pName will be
 interpreted as a POSIX Regular Expression and all parameters with a name
 matches the regular expression will have the get callback added to it.
 Use 0 otherwise.

 \return the number of parameters that have been iterated through; or the
 same as, the number of times \p callback() is called.

 */
size_t qsParameterSetterForEach(struct QsApp *app,
        struct QsBlock *block, const char *pName,
        enum QsParameterType type,
        int (*callback)(
            struct QsBlock *block, const char *pName,
            enum QsParameterType type, void *userData),
        void *userData, uint32_t flags);


/*

 TODO: add this to the docs:

 There are two modes in which quickstream modules are loaded/created:

    - **builder** we are getting description of the block and its'
        capabilities: all its' getter and setter parameters, its' optional
        fixed parameters with their default values, and it's limits on
        connecting input ports and output ports.  The block only provides
        a QsOptions structure through a getConfig() function.  The
        QsOptions structure will contain all the information needed by the
        builder program.

        QsOptions structure provides the block documentation and/or URLs
        to block documentation.

        A quickstream builder program may come in many forms, for example
        a GUI (graphical user interface) program or a command-line
        program.  The quickstream builder program may or may not be the
        same program the runs the flow in the stream graph.

        The block plugin module may provide a file named
        BLOCKNAME_getConfig.so which is a DSO (dynamic shared object)
        that contains the getConfig() function, where BLOCKNAME is the
        plugin file name that contains the modules flow-time functions.
        The benefit of separating the plugin module into two DSO files is
        that the getConfig file may not need to link to any libraries
        that may be needed for the module to run at run/flow-time.

        For super blocks that create blocks,  bla bla bla.

    - **run** we link and load more code and can run the flow graph.  In
        this mode getConfig() is called before construct().   If there is
        a getConfig DSO file, then the getConfig() function in it must
        be the same as the getConfig() in the run/flow-time plugin DSO
        file.  The optional file, named BLOCKNAME_getConfig.so, if it
        exists, is ignored in this **run** mode.
*/


/** The block plugin configuration module callback function

  The block plugin configuration module callback, \p getConfig(), is the
  only required block plugin callback function.  \p getConfig() can only
  call *builder* functions, or so called block builder functions.
  It should not access hardware devices, or initialize anything, for that
  would slow down the quickstream *builder* process.

  \param thisBlock is a pointer to the block object that the module is loaded
  for.  The passing of this argument is a little redundant given that the
  setting the block argument for more most quickstream API (application
  programming inferface) functions accept 0 as the current block.  But is does
  remind the block writer that there exists a block object and they do not
  need to create it.

  \return 0 on success, greater than 0 in the case where the DSO (dynamic shared
  object) file should be unloaded, and less than 0 if there is a error that we
  can't recover from.

 */
int getConfig(struct QsBlock *thisBlock);



int preConstruct(struct QsBlock *block);

int construct(struct QsOptions *options);

int postConstruct(struct QsBlock *block);



int work(void *in[], const size_t lenIn[],
         uint32_t numIn, uint32_t numOut);


#endif // #ifndef __qsblock_h__
