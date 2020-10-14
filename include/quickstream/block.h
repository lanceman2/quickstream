#ifndef __qsblock_h__
#define __qsblock_h__



/** \page block quickstream interfaces for blocks
 *
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






/** get a block pointer from the block name
 *
 * \param bname is the name of the block.
 *
 * \return a pointer to the block with the name.
 */
extern
struct QsBlock *qsBlockGetBlockByName(const char *bname);


/** get a block name from the block pointer
 *
 * \param block is a pointer to the block.  If block is 0
 * the current running block is used.
 *
 * \return a pointer to the block name string.
 * The returned memory is managed by the block object.
 * Do not write to it.
 */
extern
const char *qsBlockGetName(struct QsBlock *block);


/** get the number of input ports and output ports for a block
 *
 * This function is only valid after the stream has started and before the
 * stream has stopped.
 *
 * \param block is a pointer to the block.
 *
 * \param numIn if non-zero is filled with the number of input ports that
 * the block has.
 *
 * \param numOut if non-zero is filled with the number of output ports
 * that the block has.
 *
 * \return 0 on success and -1 if the block was not found.
 */
extern
int qsBlockGetNumPorts(struct QsBlock *block,
        uint32_t *numIn, uint32_t *numOut);


/** Get a pointer to a shared parameter object
 *
 * \param block the block which owns the parameter we are seeking.
 * See qsBlockGetName().  If \p block is 0 the current running block
 * will be used.
 *
 * \param pname is the name of the parameter, which is unique for a given
 * block.
 *
 * \param isGetter is true than we are looking for a getter parameter,
 * else we are looking for a setter parameter.  A getter parameter and a
 * setter parameter may have the same name.
 *
 * \return a pointer to the parameter object, or 0 if not found.
 */
extern
struct QsParameter *qsParameterGetPointer(struct QsBlock *block,
        const char *pname, bool isGetter);


/** Get the value entry size of a parameter object
 *
 * \param p is a pointer to the parameter object.
 *
 * \return the value entry size of a parameter object.
 */
extern
size_t qsParameterGetSize(struct QsParameter *p);


/** Get the name of a parameter object
 *
 * \param p is a pointer to the parameter object.
 *
 * \return the value entry size of a parameter object.
 */
extern
const char *qsParameterGetName(struct QsParameter *p);


/** Get the type of a parameter object
 *
 * \param p is a pointer to the parameter object.
 *
 * \return the type of a parameter object.
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




int work(void *in[], const size_t lenIn[],
         uint32_t numIn, uint32_t numOut);


#endif // #ifndef __qsblock_h__


