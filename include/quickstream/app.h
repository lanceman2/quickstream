#ifndef __qsgraph_h__
#define __qsgraph_h__

/** \page graph quickstream stream graph objects
 
  The libquickstream application programming interface (API)
 
  The graph object is the top level programming object of quickstream.
  We need to make an graph to run a quickstream program.  We use an
  graph object with a compiled C or C++ program that has main() in it.
  Graph is just a word chosen to be the name of the object that is the
  highest level construct the quickstream API.  quickstream users that
  are writing plugin block modules may not need to access graph objects.

 */



#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

// We wanted there to be one source to this important QUICKSTREAM_VERSION
// string.  This string must be changed to each release of the quickstream
// software package.  All other files must get this value from here.  When
// using GNU autotools, somehow configure.ac must get this string from
// here.  bootscrap can get it from here.
//
// We avoided having a graph.h.in file.  We had that in the first iteration
// of quickstream and it was a pain.
//
#define QS_MAJOR  0
#define QS_MINOR  0
#define QS_EDIT   3

//
#define STR(s) XSTR(s)
#define XSTR(s) #s

#define QUICKSTREAM_VERSION  (STR(QS_MAJOR) "." STR(QS_MINOR) "." STR(QS_EDIT))

#define QUICKSTREAM_URL   "https://github.com/lanceman2/quickstream"


/** The default number of maximum threads in a thread pool

 If a use never explicitly creates a thread pool for a graph this number
 is the default number of threads in a thread pool when the thread pool
 is created automatically when readying the flow stream.
 */
#define QS_DEFAULT_MAXTHREADS ((uint32_t) 8)


struct QsGraph;
struct QsThreadPool;
struct QsBlock;


#ifdef BUILD_LIB
#define EXPORT   __attribute__((visibility("default")))
#else
#define EXPORT  /*empty macro*/
#endif


/** Set the highest libquickstream spew level

 \param level 0 for none, 1 for error, 2 for warning, 3 for notice, 4 for info,
 and 5 for debug.
*/
EXPORT
extern
void qsSetSpewLevel(int level);


/** Get the compiled in highest spew level

 The spew level may be set with qsSetSpewLevel(), but not higher than
 what this returns.  If you need a higher spew level than this returns
 than you'll need to recompile libquickstream.

 \return 0 for none, 1 for error, 2 for warning, 3 for notice, 4 for info,
 and 5 for debug.
 */
EXPORT
extern
int qsGetLibSpewLevel(void);


/** Create an stream graph
 
  A quickstream graph is the highest level construct in the quickstream API
  (application programming interface).  Graph manages building stream graphs
  and running stream graphs.

  \return an opaque pointer to an graph object.
 */
EXPORT
extern
struct QsGraph *qsGraphCreate(void);

/** Destroy an stream graph

 
 \param graph a pointer to an graph object that was returned from qsGraphCreate().

 */
EXPORT
extern
void qsGraphDestroy(struct QsGraph *graph);


/** Add a thread pool to run the flow stream

 Thread pools (pools of threads) are used to run the flow stream graph.  We
 allow there to be more than one thread pool so that thread core affinity may
 be set with threads that are in thread pools with just one thread.

 qsGraphThreadPoolCreate() may not be called while the stream is flowing.

 \param graph is the quickstream graph object that this new thread pool will
 run blocks with.

 When the graph is destroyed the thread pool will be destroyed with it.

 \param maxThread is the maximum number of threads that will be allowed to run.
 The number of threads that can run in the pool is determined by demand.
 The threads can be thought of as flowing between blocks in the flow graph.
 If \p maxThread is 0 the main thread to run the flow stream when qsGraphFlow()
 is called.

 \param threadPoolName is the name of the thread pool.  If name is 0 and simple
 name will be generated.

 \return a pointer to an opaque thread pool object.
 */
EXPORT
extern
struct QsThreadPool *qsGraphThreadPoolCreate(struct QsGraph *graph,
        uint32_t maxThreads, const char *threadPoolName);


/** Get a thread pool object from its' name

 \param graph is the quickstream graph object.

 \param threadPoolName is a pointer to a string that is the name of the
 thread pool that we are looking for.

 \return an opaque thread pool object or 0 if one with that name was not found.
*/
EXPORT
extern struct QsThreadPool *qsGraphThreadPoolGetByName(struct QsGraph *graph,
        const char *threadPoolName);


/** Set a thread pool objects' name

 Set the name of a thread pool.

 \param tp is a pointer to the thread pool object which we wish to set the
 name of.

 \param threadPoolName is a pointer to a string that is the name of the
 thread pool that we want set the thread pool to.

 \return 0 on success, or non-zero if the name is already used by another
 thread pool.
*/
EXPORT
extern int qsThreadPoolSetName(struct QsThreadPool *tp,
        const char *threadPoolName);


/** Set the maximum number of threads that a thread pool may run

 \param tp is a pointer to the thread pool object which we wish to set the
 name of.

 \param maxThread is the maximum number of threads that will be allowed to run.
 The number of threads that can run in the pool is determined by demand.
 The threads can be thought of as flowing between blocks in the flow graph.
 If \p maxThread is 0 the main thread to run the flow stream when qsGraphFlow()
 is called.
*/
EXPORT
extern void qsThreadPoolSetMaxThreads(struct QsThreadPool *tp,
        uint32_t maxThreads);


/** Add a block to a thread pool
 
 For each block added to a thread pool, the thread pool threads will run the
 block's flow() function as the stream flows.  The threads in the pool are
 shared between all the blocks that are added.

 \param tp a pointer to a thread pool object that was returned from a call
 to qsGraphThreadPoolCreate().

 \param block who's flow() function is called by a thread in this pool, as
 the stream flows.
 */
EXPORT
extern
void qsThreadPoolAddBlock(struct QsThreadPool *tp,
        struct QsBlock *block);


/** Remove a thread pool

 Remove a thread pool from the graph that this thread pool was created for.
 The thread pool will be removed from the graph that is was created with it.

 \param tp a pointer to a thread pool object that was returned from a call
 to qsGraphThreadPoolCreate().

*/
EXPORT
extern
void qsThreadPoolDestroy(struct QsThreadPool *tp);


/** Wait for the flows to finish
 
 This call will block until the stream flow is finished.

 If there are no worker threads, qsGraphWait() will just return 0 and do
 nothing.

 \param graph a pointer to the graph object that is associated with the stream
 graph that we are waiting to finish flowing.

 \return 0 if the call waited at all, 1 if the call did not wait, and less
 than zero on error.
 */
EXPORT
extern
int qsGraphWait(struct QsGraph *graph);


/** Begin the stream flow

 Begin the stream flow by calling the flow() functions of all source blocks
 that are triggered.

 If not running with just main thread this will return and then you can
 call qsGraphWait(), otherwise this will not return until the sources dry
 up; so maybe never return without a signal.

 \param graph a pointer to the graph object that is associated with the stream
 graph.

 \return 0 on success
 */
EXPORT
extern
int qsGraphRun(struct QsGraph *graph);


/** Halt the flow

 Stop calling all flow() and trigger callback functions, even if there is
 data in the stream.  The current block callbacks that are in progress
 will finish.  If you wish to interrupt the current block callbacks that
 are in progress use a system function or system signal to do that.

 \param graph a pointer to the graph object that is associated with the stream
 graph that we want to halt the flow in.

 \return 0 on success and greater than 0 on a non-fatal error, and less
 than zero on a fatal error.
 */
EXPORT
extern
int qsGraphHalt(struct QsGraph *graph);


/** Print the name of all parameters
 
  \param graph a pointer to the graph object that is associated with the
  stream graph that we want to halt the flow in.

  \param file is a libc stream file pointer to print to.
*/
EXPORT
extern
void qsGraphParametersPrint(struct QsGraph *graph, FILE *file);


/** Print a vizgraph dot file for the graph
 
 Print a vizgraph dot file for the graph to a libc stream file.

 \param graph a pointer to the graph object that is associated with the stream
 graph that we print.  If graph is 0 than all quickstream graphs in the current
 process will be printed.

 \param file the libc stream file to print the dot graph to.  If file is 0 then
 the standard output stream will be printed to.

 \param flags print option bit flags.

 \return 0 on success and greater than zero if there is no quickstream graph, and
 less than zero on other error.
 */
EXPORT
extern
int qsGraphPrintDot(const struct QsGraph *graph, FILE *file);


EXPORT
extern
int qsGraphPrintDotDisplay(const struct QsGraph *graph, bool waitForDisplay);


EXPORT
extern
int qsBlockPrintHelp(const char *filename, FILE *file);


EXPORT
extern
uint32_t qsGraphForEachBlock(const struct QsGraph *graph,
        int (*callback)(struct QsBlock *block, void *userData));





// TODO: Worry about qsGraphKill() later.




#undef EXPORT


#endif // #ifndef __qsgraph_h__
