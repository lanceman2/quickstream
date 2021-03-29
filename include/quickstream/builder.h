#ifndef __qsbuilder_h__
#define __qsbuilder_h__


/** \page builder quickstream interfaces for building graphs of connected
  blocks
 
 Blocks have two kinds of connections.  These kinds can be classified by the
 nature of the data that flows in them:

    1. **stream** connections provide fast/continuous-like data flow.  We
        call it stream data.  The stream data flow between blocks is the
        main idea of quickstream.  Unlike GNU radio stream data flow is
        not locked in step between blocks.  The quickstream stream data
        flow is only loosely synchronised by imposing that stream buffer
        readers (inputs) do not over-run stream buffer writers (outputs).
        Stream buffers in quickstream adjust their size at flow startup
        based on the size of block stream buffer read/write promises.
        Blocks can optimize the size of read/write chunks in all blocks
        independently.  One can adjust the block stream buffer sizes to
        match device driver buffer sizes, and see dramatic performance
        improvements.  The stream data flow model allows for the same flow
        as in a GNUradio flow stream, and additional it allows for a new
        class of filter blocks that do not require a synchronised N/M flow
        between the blocks.  For example a decoder that does not have
        output for a given input until it is needed.  Can GNUradio
        uncompress video without sending blank/dummy data?  I don't know.

    2. **parameter** connections provide thread-safe asynchronous
        relatively slow and discrete data transfer between blocks.  \e
        parameters are always copied from one block to other blocks.  The
        closest analogy of quickstream \e paramters in GNUradio is Message
        Passing API https://wiki.gnuradio.org/index.php/Message_Passing .
        quickstream does not directly allow blocks to expose functions
        of blocks to outside the block, there-as GNUradio exposes lots of
        setters and getter methods that are not thread safe.  By
        restricting the quickstream block interfaces quickstream leads to
        forcing blocks to using standard and thread safe \e parameter
        setting and getting interfaces, which has the additional benefit
        forcing all blocks to be seamlessly connected.  Blocks in
        quickstream do not need to know particular methods in order to
        control other blocks.  This leads to universal interfaces for all
        blocks, and generic parameter extensions.  One can use a very
        simple block to expose all parameters in a running flow stream to
        web clients without any coding of the blocks that are being
        controlled.  quickstream is designed for the IoT (Internet of
        Things).

 Comparing rates of stream data and parameter data: stream data is
 transferred at about one thousand times faster than parameter data.  Of
 course that is a very gross statement but it is easy to see in common
 example: audio stream could be transported at about 8 bytes at 40 kHz
 giving 8*40*1000 bytes/second = 320,000 bytes/second and a volume
 adjustment parameter will change, at high end, 4 bytes in a tenth of a
 second, giving a parameter data rate of 40 bytes/second.  The ratio of
 the stream to parameter rates being 320,000/40 = 8,000.  So stream data
 in this case is 8,000 times faster than parameter control data.  We
 expect the stream rates would be a lot higher and that parameter change
 rates will be much lower.  In many uses the parameter transfer rate will
 be zero for long periods of time.  So ya, parameters change slowly
 compared to stream data, if not than you are using quickstream for the
 wrong thing.


 Blocks in quickstream do not have any non-standard quickstream
 interfaces.  There-as GNUradio lets blocks expose any block defined
 public method.  One can truly think of quickstream blocks as "black
 boxes" with very restricted interfaces between them.  We consider this
 lack of inter-block constraint of GNUradio to tend to paralyse future
 development and extension.  Most of the GNUradio blocks have there guts
 hanging out.  It shows a lack of foresight in the design of GNUradio.
 Blocks in quickstream do not have there guts hanging out.  This
 simplifies quickstream.  The bottom-line is, can quickstream do what you
 need with this very restricted interface?  We'll see.


 The quickstream builder interface functions provide ways to create and
 connect blocks together.  There is no builder class object.  There is an
 opaque graph (QsGraph) object that contains lists of blocks and block
 connections.  This builder is just the "graph building" subset of
 functions that create and connect blocks together.  Having a builder
 object was found to just add complexity.  Since the graph object had to
 have all of the blocks and connections in it anyway, the builder
 is built into the graph object.

 That left us with three basic API user opaque objects (data structures):

    1. **QsGraph** factory for all blocks and connections that can build and run
        the flow graph that it builds
    2. **QsBlock** basic processing module that we connect to build flow
        graphs
    3. **QsParameter** named discrete-like controlling connections


 The stream connections do not require an object to be exposed, we just connect
 blocks to port numbers.

 */


#define QS_DEFAULT_INPUT_THRESHOLD  (1)
#define QS_DEFAULT_INPUT_MAXREAD    (1024) // maximum input read promise
#define QS_DEFAULT_OUTPUT_MAXWRITE  (1024)


 

struct QsGraph;
struct QsBlock;
struct QsParameter;


/** bit flag used to mark the use of regular expressions to find
 * parameter with qsParameterForEach() function.
 */
#define QS_PNAME_REGEX     (01)
/** bit flag used to mark the use case insensitive regular expressions to
 * find parameter with qsParameterForEach() function.  The QS_PNAME_REGEX
 * flag must be set too.
 */
#define QS_PNAME_ICASE     (02)



#ifdef BUILD_LIB
#define EXPORT   __attribute__((visibility("default")))
#else
#define EXPORT  /*empty macro*/
#endif



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
EXPORT
extern
int qsBlockGetNumPorts(const struct QsBlock *block,
        uint32_t *numIn, uint32_t *numOut);



/** get the graph that the block was loaded with
 
 \param block the block

 \return a pointer to the graph that loaded and owns block \p block.

 */
EXPORT
extern
struct QsBlock *qsBlockGetGraph(const struct QsBlock *block);



/** get a block pointer from the block name

 \param graph the graph that the block was loaded with.

 \param bname is the name of the block.

 \return a pointer to the block with the name.
 */
EXPORT
extern
struct QsBlock *qsGraphGetBlockByName(const struct QsGraph *graph,
        const char *bname);




// Called when graph is paused.  Parameter, p, must be a constant or a
// getter.  Setter values can only get set by connecting them.
//
// The stream must be paused.
//
// /return 0 on success, and -1 if the parameter is a setter.
//
EXPORT
extern
int qsParameterSetValue(struct QsParameter *p, const void *value);


/** connect two different block's parameters together

 Connect parameters from two different blocks.  The value goes from the
 \p from a setter parameter's block to the \p to a getter parameter's
 block.

 Sets up a data exchange from a getter parameter to a setter parameter, a
 constant parameter to a setter parameter, or a constant parameter to
 another constant parameter.  Once a constant parameter is in the
 connected graph of parameters the value of all parameters in the group
 will be constant at flow-time.

 Each time the getter parameter changes the value is queued up and
 copied to the setter parameter.

 \param from pointer to the getter parameter to get values from or
 a constant parameter.  \p from may not be a pointer to a setter
 parameter.

 \param to pointer to a setter parameter to be set with values from
 the other \p from parameter.  This may also be constant parameter
 if \p from is a pointer to another constant parameter.

 We may not connect a getter parameter to a constant parameter.

 \return 0 on success, 1 if the \p to setter parameter is already
 connected to a getter parameter, or -1 on other failed cases.
 When compiled with DEBUG the other failed cases will call assert.

 */
EXPORT
extern
int qsParameterConnect(struct QsParameter *from, struct QsParameter *to);


/** Disconnect a parameter

 \param p parameter to disconnect from.

 \param p1 if set will be the only parameter disconnected from all
 parameters that are connected to this parameter connection group.  If \p
 p1 is not set parameter \p p will disconnect from all other parameters in
 the graph.

 */
EXPORT
extern
void qsParameterDisconnect(struct QsParameter *p, struct QsParameter *p1);


/** Super block exposing a parameter of an inner block

 */
EXPORT
extern
int qsParameterExpose(struct QsBlock *superBlock, const char *pname,
        struct QsParameter *parameter);


/** Super block exposing an input port of an inner block

 */
EXPORT
extern
int qsBlockExposeInput(struct QsBlock *superBlock, uint32_t exposeInPort,
        struct QsBlock *block, uint32_t inPort);


/** Super block exposing an output port of an inner block

 */
EXPORT
extern
int qsBlockExposeOutput(struct QsBlock *superBlock, uint32_t exposeOutPort,
        struct QsBlock *block, uint32_t outPort);


/** connect the output of one block to the input of another block
 
  \param from connect from block \p from

  \param to connect to block \p to

  \param outPortNum the output port number to connect from.  Output
  ports may have any number of connections.

  \param inPortNum the input port number to connect from.  Input ports may
  only have one connection.

  Before a flow can start the all output and input ports numbers that are
  connected must make a complete counting sequence starting from 0 to the
  number of connections minus one.  That's 0 to N-1, where N is the number
  of connections, for input and output.

  \return 0 on success, or less than 0 on error and the connection is not
  made.

 */
EXPORT
extern
int qsBlockConnect(struct QsBlock *from, struct QsBlock *to,
        uint32_t outPortNum, uint32_t inPortNum);


EXPORT
extern
int qsBlockDisconnect(struct QsBlock *toBlock, uint32_t inputPortNum);


/** Load a block plugin DSO (dynamic shared object) and call getConfig()

  Find and load the DSO file and call the getConfig() for that block.
  This creates a quickstream block.

  \param graph the graph that this block will belong to.
  \param fileName that refers to the plugin module file.
  \param blockName a made-up unique name that was refer to this
  loaded plugin module.  loadName=0 maybe passed in to have the
  unique name generated based on filename.

  \return a pointer to the block or 0 on error.
 */
EXPORT
extern
struct QsBlock *qsGraphBlockLoad(struct QsGraph *graph, const char *fileName,
        const char *blockName);



EXPORT
extern
void qsBlockUnload(struct QsBlock *block);


/** Iterate through the parameters via a callback function
 
 \param graph is ignored unless \p block is 0.  If \p graph is non-zero
 and \p block is zero, all blocks are iterated through in all selected
 setter parameters in this \p graph.

 \param block if block is not 0, restrict the range of the getter
 parameters to iterate through to just setter parameters owned by this
 block.

 One of arguments \p graph or \p block must be non-zero.

 \param pName if not 0, restrict the range of the parameters to iterate
 through just parameters with this name.

 \param kind is not QsAny, restrict the kind of the parameters to iterate
 through just parameters with this kind, otherwise through parameters of
 any kind: Constants, Getters, and Setters.

 \param type if not 0, restrict the range of the parameters to iterate
 through just parameters with this type.

 \param size if not 0, restrict the size of the parameters to iterate
 through just parameters with this size.

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
 Returns -1 on error.

 */
EXPORT
ssize_t qsParameterForEach(struct QsGraph *graph,
        struct QsBlock *block,
        enum QsParameterKind kind,
        enum QsParameterType type,
        size_t size,
        const char *pName,
        int (*callback)(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, void *userData),
        void *userData, uint32_t flags);


#undef EXPORT

#endif // #ifndef __qsbuilder_h__
