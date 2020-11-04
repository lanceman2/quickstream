#ifndef __qsapp_h__
#define __qsapp_h__

/** \page app quickstream application objects
 
  The libquickstream application application programming interface (API)
 
  The app object is the top level programming object of quickstream.
  We need to make an app to run a quickstream program.  We use an
  app object with a compiled C or C++ program that has main() in it.
  App is just a word chosen to be the name of the object that is the
  highest level construct the quickstream API.  quickstream users that
  are writing plugin block modules do not need to access app objects.

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
// We avoided having a app.h.in file.  We had that in the first iteration
// of quickstream and it was a pain.
//
#define QS_MAJOR  0
#define QS_MINOR  0
#define QS_EDIT   3

//
#define STR(s) XSTR(s)
#define XSTR(s) #s

#define QUICKSTREAM_VERSION  (STR(QS_MAJOR) ":" STR(QS_MINOR) ":" STR(QS_EDIT))

#define QUICKSTREAM_URL   "https://github.com/lanceman2/quickstream"


struct QsApp;
struct QsThreadPool;
struct QsBlock;


/** Set the highest libquickstream spew level

 \param level 0 for none, 1 for error, 2 for warning, 3 for notice, 4 for info,
 and 5 for debug.
*/
extern
void qsSetSpewLevel(int level);


/** Get the compiled in highest spew level

 The spew level may be set with qsSetSpewLevel(), but not higher than
 what this returns.  If you need a higher spew level than this returns
 than you'll need to recompile libquickstream.

 /return 0 for none, 1 for error, 2 for warning, 3 for notice, 4 for info,
 and 5 for debug.
 */
extern
int qsGetLibSpewLevel(void);


/** Create an app
 
  A quickstream app is the highest level construct in the quickstream API
  (application programming interface).  App manages building stream graphs
  and running stream graphs.

  \return an opaque pointer to an app object.
 */
extern
struct QsApp *qsAppCreate(void);

/** Destroy an app

 


 /param app a pointer to an app object that was returned from qsAppCreate().

 */
extern
void qsAppDestroy(struct QsApp *app);


/** Add a thread pool to run the flow stream

 Thread pools (pools of threads) are used to run the flow stream graph.  We
 allow there to be more than one thread pool so that thread core affinity may
 be set with threads that are in thread pools with just one thread.

 qsAppThreadPool() may not be called while the stream is flowing.

 /param app is the quickstream app object that this new thread pool will
 run blocks with.

 When the app is destroyed the thread pool will be destroyed with it.

 /param maxThread is the maximum number of thread that will be allowed to run.
 The number of threads that can run in the pool is determined by demand.
 The threads can be thought of as flowing between blocks in the flow graph.
 If \p maxThread is 0 the main thread to run the flow stream when qsAppFlow()
 is called.

 \return a pointer to an opaque thread pool object.
 */
extern
struct QsThreadPool *qsAppThreadPool(struct QsApp *app, uint32_t maxThreads);


/** Add a block to a thread pool
 
 For each block added to a thread pool, the thread pool threads will run the
 block's work() function as the stream flows.  The threads in the pool are
 shared between all the blocks that are added.

 /param tp a pointer to a thread pool object that was returned from a call
 to qsAppThreadPool().

 /param block who's work() function is called by a thread in this pool, as
 the stream flows.
 */

extern
void qsThreadPoolAddBlock(struct QsThreadPool *tp,
        struct QsBlock *block);

/** Remove a thread pool

 Remove a thread pool from the app that this thread pool was created for.
 The thread pool will be removed from the app that is was created with it.

 \param tp a pointer to a thread pool object that was returned from a call
 to qsAppThreadPool().

*/

extern
void qsThreadPoolRemove(struct QsThreadPool *tp);


/** Ready the flow graph
 
 Allocate the stream buffers and call the block start() functions, but
 does not run the flow.

 /param app a pointer to an app.

 /return 0 on success
 */
extern
int qsAppReady(struct QsApp *app);


// If not running with just main thread this will return and then you can
// call qsAppWait(), otherwise this will not return until the sources dry
// up; so maybe never return without a signal and stuff.

/** Begin the stream flow

 Begin the stream flow by calling the work() functions of all source blocks
 that are triggered.

 /param app a pointer to the app object that is associated with the stream
 graph.
  
 /return 0 on success
 */
extern
int qsAppFlow(struct QsApp *app);


// stop feeding the sources, wait for full flush, interrupt file
// descriptor polling, cond_signal all worker threads, wait for all
// threads to return, and then return with just main thread running.
extern
int qsAppStop(struct QsApp *app);


// run flow until it finishes.  Or wait for work() to return after
// qsAppHalt().
extern
int qsAppWait(struct QsApp *app);


// Stop calling all work() functions, even if there is data in the stream.
extern
int qsAppHalt(struct QsApp *app);


extern
int asAppPrintDot(struct QsApp *app, uint32_t flags);


extern
int qsBlockPrintHelp(const char *filename, FILE *file);


extern
uint32_t qsAppForEachBlock(struct QsApp *app,
        int (*callback)(struct QsBlock *block, void *userData));





// TODO: Worry about qsAppKill() later.



#endif // #ifndef __qsapp_h__
