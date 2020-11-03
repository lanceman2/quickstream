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


extern
struct QsApp *qsAppCreate(void);


extern
void qsAppDestroy(struct QsApp *app);


// maxThreads = 0 means use main thread to run qsAppFlow()
extern
struct QsThreadPool *qsAppAddThreadPool(uint32_t maxThreads);


extern
struct QsThreadPool *qsThreadPoolAddBlock(struct QsThreadPool *tp,
        struct QsBlock *block);


extern
struct QsThreadPool *qsThreadPoolremove(struct QsThreadPool *tp);


extern
void qsThreadPoolDestroy(struct QsThreadPool *tp);


extern
int qsAppReady(struct QsApp *app);


// If not running with just main thread this will return and then you can
// call qsAppWait(), otherwise this will not return until the sources dry
// up; so maybe never return without a signal and stuff.
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
