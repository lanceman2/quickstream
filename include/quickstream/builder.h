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
        readers (inputs) do not over-run stream buffer writers
        (outputs).
    2. **parameter** connections provide thread-safe asynchronous slow and
        discrete data transfer between blocks.  \e parameters are always
        copied from one block to another.  The closest analogy of
        quickstream \e paramters in GNUradio is Message Passing API
        https://wiki.gnuradio.org/index.php/Message_Passing .  quickstream
        does not directly allow blocks to expose functions of blocks to
        outside the block, there-as GNUradio exposes lots of setters and
        getter methods that are not thread safe.  By restricting the
        quickstream block interfaces quickstream leads to forcing blocks
        to using standard and thread safe \e parameter setting and getting
        interfaces, which has the additional benefit forcing all blocks to
        be seamlessly connected.  Blocks in quickstream do not need to
        know particular methods in order to control other blocks.  This
        leads to universal interfaces for all blocks, and generic
        parameter extensions.  One can use a very simple block to expose
        all parameters in a running flow stream to web clients without any
        coding of the blocks that are being controlled.  quickstream is
        designed for the IoT (Internet of Things).

 Blocks in quickstream do not have any non-standard quickstream
 interfaces.  There-as GNUradio lets blocks expose any block defined
 public method.  One can truly think of quickstream blocks as "black
 boxes" with very restricted interfaces between them.  We consider this
 lack of inter-block constraint of GNUradio to tend to paralyse future
 development and extension.  Most of the GNUradio blocks have there guts
 hanging out.  It's shows a lack of foresight in the design of GNUradio.
 Blocks in quickstream do not have there guts hanging out.  This
 simplifies quickstream.  The bottom-line is, can quickstream do what you
 need?  We'll see.

 */
 

struct QsApp;
struct QsBlock;
struct QsParameter;



/** connect two different block's parameters together

 Connect parameters from two different blocks.  The value goes from the
 \p from a setter parameter's block to the \p to a getter parameter's
 block.

 Sets up a data exchange from a getter parameter to a setter parameter.
 Each time the getter parameter changes the value is queued up and
 copied to the setter parameter

 \param from pointer to the getter parameter to get values from.

 \param to pointer to the setter parameter to be set with values from
  the getter parameter.

 \return 0 on success, 1 if the \p to setter parameter is already
 connected to a getter parameter, or -1 on other failed cases.
 When compiled with DEBUG the other failed cases will call assert.

 */
extern
int qsParameterConnect(struct QsParameter *from,
        struct QsParameter *to);


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
extern
int qsBlockConnect(struct QsBlock *from, struct QsBlock *to,
        uint32_t outPortNum, uint32_t inPortNum);


/** Load a block plugin DSO (dynamic shared object) and call getConfig()

  Find and load the DSO file and call the getConfig() for that block.
  This creates a quickstream block.

  \param app the app that this block will belong to.

  \return a pointer to the block or 0 on error.

 */
extern
struct QsBlock *qsAppLoadBlock(struct QsApp *app);


#endif // #ifndef __qsbuilder_h__
