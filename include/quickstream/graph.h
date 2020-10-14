#ifndef __qsgraph_h__
#define __qsgraph_h__




/** \page graph quickstream interfaces for building graphs of connected
 * blocks
 *
 */
 

struct QsGraph;
struct QsParameter;


/** connect two parameters together
 *
 * Connect parameters from two different blocks.  The value goes from the
 * \p from block to the \p to block
 *
 * Sets up a data exchange from a getter parameter to a setter parameter.
 * Each time the getter parameter changes the value is queued up and
 * copied to the setter parameter
 *
 * \param from pointer to the getter parameter to get values from.
 *
 * \param to pointer to the setter parameter to be set with values from
 *  the getter parameter.
 * 
 * \return 0 on success
 */
extern
int qsParameterConnect(struct QsParameter *from,
        struct QsParameter *to, uint32_t flags);





#endif // #ifndef __qsgraph_h__
