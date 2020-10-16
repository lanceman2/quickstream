#ifndef __qsgraph_h__
#define __qsgraph_h__




/** \page graph quickstream interfaces for building graphs of connected
 * blocks
 *
 */
 

struct QsApp;
struct QsBlock;
struct QsParameter;



/** connect two different block's parameters together

 Connect parameters from two different blocks.  The value goes from the
 \p from setter parameter's block to the \p to getter parameter's block.

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







#endif // #ifndef __qsgraph_h__
