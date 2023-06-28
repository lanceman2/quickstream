// We have code for thread pool worker thread "graph commands" in here.
//
// The thread pool worker threads can't just edit the graph as the worker
// threads are running, so we have a "round-about" way to have a thread
// pool worker thread "edit the graph" asynchronously.
//
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"
#include "name.h"

#include "c-rbtree.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"



// We keep a list of these in struct QsGraph.
//
// We managed to keep this struct hidden (opaque) in this file. :)
//
struct QsCommand {

    // Particular command structures must inherit this struct at the top.

    // A function that is called in the thread that is calling
    // qsGraph_wait(), that is not a thread pool worker thread.
    //
    // This function is called after getting the graph mutex lock.
    //
    // userData will be a pointer to this struct.
    //
    int (*callback)(struct QsGraph *g,
            struct QsBlock *b, struct QsCommand *c);

    // The block that is asynchronously calling this callback function.
    //
    // i.e. it calls it after the thread pool is halted.
    struct QsBlock *block;

    struct QsCommand *next, *prev;

    // Size of memory of a struct that inherits this QsCommand.
    size_t size;
};



// The returned pointer must be freed if it is not 0.
//
// We must have a g->cqMutex lock to call this.
//
static inline
struct QsCommand *PopCommand(struct QsGraph *g) {

    DASSERT(g);

    struct QsCommand *c = g->firstCommand;

    if(!c) return c;

    DASSERT(!c->prev);

    // Remove the command from the graphs command list:
    if(c->next) {
        DASSERT(c->next->prev == c);
        DASSERT(c != g->lastCommand);
        c->next->prev = 0;
    } else {
        DASSERT(c == g->lastCommand);
        g->lastCommand = 0;
    }
    g->firstCommand = c->next;
    c->next = 0;

    return c;
}


static inline
void FreeGraphCommand(struct QsCommand *c) {

    DASSERT(c);
    DASSERT(c->size);

    // Now cleanup memory for this command.
    DZMEM(c, c->size);

    free(c);
}


// We must have a g->cqMutex lock to call this.
//
void FreeGraphCommands(struct QsGraph *g) {

    DASSERT(g);
    for(struct QsCommand *c = PopCommand(g); c; c = PopCommand(g))
        FreeGraphCommand(c);
}



// This is running a command (function) that came from a thread pool
// worker that a job work() function called.  Like for example: a job
// work() function called qsDestroy(), that is a block callback function
// called qsDestroy(), but it can't call it directly because that would
// hose all the thread pool worker threads.  So it queues a request that
// calls it in another thread that is not a thread pool worker thread
// which is the thread calling qsGraph_wait().
//
static inline int RunCommand(struct QsGraph *g, struct QsCommand *c) {

#ifdef DEBUG
    NotWorkerThread();
#endif

    DASSERT(g);
    DASSERT(c);
    DASSERT(c->block);
    DASSERT(c->callback);

    return c->callback(g, c->block, c);
}


// We have a g->cqMutex lock before calling this.
//
static inline int RunCommands(struct QsGraph *g) {

#ifdef DEBUG
    // This should already have been checked.
    NotWorkerThread();
#endif

    DASSERT(g);

    // Run the commands that are in the graph command queue.
    //
    struct QsCommand *c = PopCommand(g);
    DASSERT(c);
    
    int ret;

    do {
        CHECK(pthread_mutex_unlock(&g->cqMutex));

        ret = RunCommand(g, c);
        FreeGraphCommand(c);

        CHECK(pthread_mutex_lock(&g->cqMutex));
    } while(!ret && (c = PopCommand(g)));


    return ret;
}


static bool inline
CheckStreamFinish(struct QsGraph *g) {

    bool ret = false;

    DASSERT(!g->destroyingGraph);

    CHECK(pthread_mutex_unlock(&g->cqMutex));

    // After the above unlock the state of g->destroyingGraph
    // can change.

    CHECK(pthread_mutex_lock(&g->mutex));

    if(g->runningStreams && !g->streamJobCount) {
        qsGraph_stop(g);
        DASSERT(!g->runningStreams);
        ret = true;
    }

    CHECK(pthread_mutex_unlock(&g->mutex));

    CHECK(pthread_mutex_lock(&g->cqMutex));

    return ret;
}


static inline bool IsNotTimed(double seconds) {

    return (seconds <= 0.0 || !isnormal(seconds) || seconds > 1.0e12);
}


// Timeout or got non-destroy event return 0
//
// graph destroy return 1
//
// This relinquishes control of the graph to the blocks.  The blocks can't
// control the graph unless this is called.  The blocks submit requests to
// this thread via the graph command queue in
// QsGraph::QsCommand::firstCommand controlling access with
// QsGraph::cqMutex.
//
// Interest case is: Q: what happens if many threads run this
// concurrently?  A: I think it might work fine; but it's not what is an
// intended use.  There would not be any benefit to using such a run case.
// TODO: We maybe should test that it works in that case, given we do not
// exclude it in the design.
//
int qsGraph_wait(struct QsGraph *g, double seconds) {

    NotWorkerThread();

    DASSERT(g);
    DSPEW("seconds=%g", seconds);
    errno = 0;
    bool doneWaiting = false;

    int ret = 1;

    CHECK(pthread_mutex_lock(&g->cqMutex));

    while(!g->destroyingGraph) {
        // The stream finishing running do to "running to completion" is a
        // special event case, given it is not caused by a graph
        // edit/control command from a master thread/high level graph
        // runner user.
        bool check = CheckStreamFinish(g);

        // The state of g->destroyingGraph could have changed in
        // CheckStreamFinish().
        if(g->destroyingGraph)
            break;

        if(g->firstCommand) {
            // Because g->firstCommand is set, there are commands in the
            // graph command queue.
            //
            // This is what we were waiting for; running graph
            // asynchronous edit/control commands that the thread pool
            // workers request.
            int destroyingGraph = RunCommands(g);

            CHECK(pthread_mutex_unlock(&g->cqMutex));

            if(destroyingGraph) {
                // We must do this last because it makes the graph data go
                // away.
                qsGraph_destroy(g);
                // We have no g->cqMutex lock.  All the graph mutexes are
                // unusable now anyway.
                return 1;
            }
            // We have no g->cqMutex lock:
            return 0;
        }

        if(check || doneWaiting) {
            ret = 0;
            goto finish;
        }

        g->waiting = true;

        if(IsNotTimed(seconds)) {
            CHECK(pthread_cond_wait(&g->cqCond, &g->cqMutex));
        } else {
            // I do not like it that pthread_cond_timedwait() uses absolute
            // time.
            struct timeval at;
            ASSERT(0 == gettimeofday(&at, 0));
            struct timespec t;
            t.tv_sec = seconds;
            t.tv_nsec = (seconds -  ((double) t.tv_sec)) * 1.0e9;

            t.tv_sec += at.tv_sec;
            t.tv_nsec += at.tv_usec * 1.0e3;

            if(t.tv_nsec > 1000000000) {
                t.tv_nsec -= 1000000000;
                t.tv_sec += 1;
            }
            //DSPEW("waiting %lu seconds %lu nanosec", t.tv_sec, t.tv_nsec);
            pthread_cond_timedwait(&g->cqCond, &g->cqMutex, &t);
            // For the timed wait we just wait once no matter what time it
            // waited, even when it wakes up from a signal.
        }
        g->waiting = false;
        doneWaiting = true;
DSPEW();
    }

finish:

    CHECK(pthread_mutex_unlock(&g->cqMutex));

    return ret; // 1 =>> This graph is being destroyed.
}


// seconds as a double
static inline double
GetTimeDiff(struct timeval *past) {

    struct timeval now;
    ASSERT(0 == gettimeofday(&now, 0));

    return (double) (now.tv_sec - past->tv_sec) +
        (now.tv_usec - past->tv_usec)/1000000.0;
}


// Return 1 if the graph is destroyed.
//
// Return 0 if it times out.
int qsGraph_waitForDestroy(struct QsGraph *g, double seconds) {

    NotWorkerThread();

    DASSERT(g);

    if(IsNotTimed(seconds)) {
        while(qsGraph_wait(g, seconds) != 1);
        return 1;
    }

    struct timeval past;
    ASSERT(0 == gettimeofday(&past, 0));

    while(qsGraph_wait(g, seconds) != 1) {
        if(GetTimeDiff(&past) >= seconds)
            return 0;
    }

    return 1;
}


// Return 1 if the graph is destroyed.
int qsGraph_waitForStream(struct QsGraph *g, double seconds) {

    NotWorkerThread();

    DASSERT(g);

    bool runningStreams;

    struct timeval past;

    if(!IsNotTimed(seconds))
        ASSERT(0 == gettimeofday(&past, 0));

    CHECK(pthread_mutex_lock(&g->mutex));
    runningStreams = g->runningStreams;
    CHECK(pthread_mutex_unlock(&g->mutex));

    while(runningStreams) {
        if(qsGraph_wait(g, seconds) == 1)
            return 1; // The graph is destroyed.

        if(!IsNotTimed(seconds) && GetTimeDiff(&past) >= seconds)
            // It timed out.
            return 0;

        CHECK(pthread_mutex_lock(&g->mutex));
        runningStreams = g->runningStreams;
        CHECK(pthread_mutex_unlock(&g->mutex));
    }

    return 0;
}


static inline void
QueueAndSignalCommand(struct QsGraph *g, struct QsCommand *c,
        struct QsBlock *b, int (*callback)(struct QsGraph *g,
            struct QsBlock *b, struct QsCommand *c),
        size_t size) {

    DASSERT(g);
    DASSERT(c);
    DASSERT(b);
    DASSERT(size);
    DASSERT(!c->next);
    DASSERT(!c->prev);

    c->block = b;
    c->callback = callback;
    c->size = size;

    // Add to the list of this graph's commands as the last in the
    // queue.
    CHECK(pthread_mutex_lock(&g->cqMutex));

    if(g->destroyingGraph) {
        // We are about to, or in the process of destroying the graph so
        // we can't run any more graph commands.  Also, queuing a command
        // now will allocate memory for the queue that has no way of being
        // freed.  Valgrind found this as a memory leak bug.  It's fixed
        // by this if() goto here.  It was a race condition type bug that
        // only happened sometimes in tests/520_parameterAddLoops.  Hence
        // this long comment.
        //
        // I thought it was a bug in parameter.c.  It drove me a little
        // bit nuts.
        FreeGraphCommand(c);
        goto finish;
    }


    c->prev = g->lastCommand;

    if(g->firstCommand) {
        DASSERT(g->lastCommand);
        g->lastCommand->next = c;
    } else {
        DASSERT(!g->lastCommand);
        g->firstCommand = c;
    }

    g->lastCommand = c;

    if(g->waiting) {
        DSPEW("Signaling the qsGraph_wait() thread from worker thread");
        // This is the only place, in the whole code, that we signal
        // this thread:
        CHECK(pthread_cond_signal(&g->cqCond));
    }

finish:

    CHECK(pthread_mutex_unlock(&g->cqMutex));
}


static int
Destroy(struct QsGraph *g, struct QsBlock *b, struct QsCommand *c) {

#ifdef DEBUG
    NotWorkerThread();
#endif

    DASSERT(g);
    DASSERT(b);
    DASSERT(c);
    DASSERT(c->callback == Destroy);

    DSPEW("Thread pool worker for block \"%s\" causing graph destroy",
            b->name);
    // We cannot call qsGraph_destroy(g) because we need the
    // graph data structure in order to return from qsGraph_wait(),
    // so we just set a flag for now:
    return 1;
}


// Some kind of async-graph-destroy for blocks running in a thread pool
// worker thread.
//
// The graph cleanup happens in the call to qsGraph_wait() in another
// thread that is not a thread pool thread.  In effect the thread that
// calls qsGraph_wait() is the quickstream "master thread".
//
// In this command there is no feedback to the block code; hence no
// function callback.
//
void qsDestroy(struct QsGraph *g, int status) {

    struct QsWhichJob *wj = pthread_getspecific(threadPoolKey);

    // For now anyway:
    //
    // Interesting note about exit (from code that was removed):
    //
    // Calling exit(3) in main() just works fine, the libquickstream
    // destructor cleans up everything.  If we are assuming that other
    // threads do not call exit(3) is thread-safe to call exit() in
    // main(), otherwise it is not thread-safe.
    //
    ASSERT(wj, "Only jobs blocks can call this");

    // This is the case where a thread pool worker thread is calling
    // this function.  We cannot just do what is asked of us now,
    // because that would leave "uncleaned" system resources while
    // doing these commands.  Like for example tests run with valgrind
    // would fail; and some blocks with middle-ware drivers will not
    // get destructors called, possibly causing the next "start" to
    // fail.
    //
    // i.e. We are trying to make this a "clean-destroy" for the
    // example case when a thread pool worker is trying to destroy a
    // graph.
    DASSERT(wj->job);
    DASSERT(wj->threadPool);
    DASSERT(wj->job->jobsBlock);
    // A block can only destroy it's own graph, which is 0 in the
    // block code.  A block code should never directly get a graph
    // pointer, so ya 0 is it's graph.
    ASSERT(g == 0);

    g = wj->job->jobsBlock->block.graph;
    DASSERT(g);

    struct QsCommand *c = calloc(1, sizeof(*c));
    ASSERT(c, "calloc(1,%zu) failed", sizeof(*c));

    QueueAndSignalCommand(g, c, (void *) wj->job->jobsBlock,
            Destroy, sizeof(*c));
}


static int
Start(struct QsGraph *g, struct QsBlock *b, struct QsCommand *c) {

#ifdef DEBUG
    NotWorkerThread();
#endif

    DASSERT(g);
    DASSERT(b);
    DASSERT(c);
    DASSERT(c->callback == Start);

    DSPEW("Thread pool worker for block \"%s\" causing graph start",
            b->name);

    qsGraph_start(g);
    return 0;
}


void qsStart(void) {

    // This is a worker thread calling this.
    struct QsWhichJob *wj = pthread_getspecific(threadPoolKey);
    ASSERT(wj, "Only jobs blocks can call this");
    struct QsGraph *g = wj->job->jobsBlock->block.graph;
    DASSERT(g);

    DSPEW("Block \"%s\" requesting stream start",
            wj->job->jobsBlock->block.name);

    struct QsCommand *c = calloc(1, sizeof(*c));
    ASSERT(c, "calloc(1,%zu) failed", sizeof(*c));

    QueueAndSignalCommand(g, c, (void *) wj->job->jobsBlock,
            Start, sizeof(*c));
}


static int
Stop(struct QsGraph *g, struct QsBlock *b, struct QsCommand *c) {

#ifdef DEBUG
    NotWorkerThread();
#endif

    DASSERT(g);
    DASSERT(b);
    DASSERT(c);
    DASSERT(c->callback == Stop);

    DSPEW("Thread pool worker for block \"%s\" causing graph stop",
            b->name);

    qsGraph_stop(g);
    return 0;
}


void qsStop(void) {

    // This is a worker thread calling this.
    struct QsWhichJob *wj = pthread_getspecific(threadPoolKey);
    ASSERT(wj, "Only jobs blocks can call this");
    struct QsGraph *g = wj->job->jobsBlock->block.graph;
    DASSERT(g);

    DSPEW("Block \"%s\" requesting stream stop",
            wj->job->jobsBlock->block.name);

    struct QsCommand *c = calloc(1, sizeof(*c));
    ASSERT(c, "calloc(1,%zu) failed", sizeof(*c));

    QueueAndSignalCommand(g, c, (void *) wj->job->jobsBlock,
            Stop, sizeof(*c));
}

// I love to ramble and rant.  This is a total fucking rambling rant:
//
// The program quickstreamGUI has the "two center of the universes"
// problem.  It uses one center of the universe the GTK3 main loop and the
// other is in libquickstream.so.   libquickstream.so requires that a
// program that "runs" quickstream graphs have a main loop that is
// constructed with qsGraph_wait().  Hence we need to mash together two
// main loops.  There is a idle time callback thingy in GTK3, but we
// refuse to write shitty code the uses CPU when it clearly should not
// be using any CPU.  Down the road such a method would tend to lead to
// programs that unnecessarily eat computer system resources.  I consider
// such like programs to be "not robust".  So, I resolve the problem by
// adding an additional thread.  I hate doing that, but it was either
// that or adding a shit ton of complexity to the thread pool worker main
// loops.  I think that the thread pool worker main loop function looks
// pretty dam elegant, and the idea of adding another mutex lock and
// unlock to the loop seemed like a performance spoiler.
//
// NOTE: GTK3 used to have a more robust solution for this problem, but
// they removed the interface from the API.  It was a little convoluted,
// but it was robust as I see it.  You pass in a condition variable and a
// mutex to the "main looper" thingy and you control it with that.
//
// If you find something in quickstream that is not "robust" I want to
// know about it.  If I can't fix it, I'll at least document it as a
// limitation.  I'll at least strive to make it robust like an operating
// system kernel.


//
// Some APIs (application programming interfaces), or so called libraries,
// add threads willy nilly.  I don't do that in libquickstream.so.  I add
// this thread with great reluctance.  libquickstream.so does not add
// threads to programs willy nilly.  One of the main design criteria of
// quickstream is the minimization of threads; and hence the end user has
// control over how thread pool worker threads are distributed over the
// block modules.
//
//
// There were 2 possible forms for this (these) function(s):
//
//    1. build it on top of existing, now simple, qsGraph_wait(), or
//
//    2. do a major refactoring of qsGraph_wait() and the hocks in other
//       related functions, and have it behave differently depending on
//       options.
//
//
// Here's a try at 1.  2 functions: enter and exit.  enter is called
// before calling a series of graph edit functions, and exit is called
// after calling a series of graph edit functions; in the time between the
// exit and the enter the qsGraph_wait(g) is called for all existing
// graphs.  We think of "enter" as becoming the quickstream master thread
// and "exit" and not being the quickstream master thread anymore.
//
// The word "lock" was already taken by qsGraph_lock() and
// qsGraph_unlock(), which are related to this but not the same; as you
// can see in this code.
//

// As you can see below, [1] implemented here is super fucking simple.
// Simple is good.  I'm guessing [1] being this simple would be better
// than [2] by many other measures too.
//
// Note: with this stuff like this, we can still have good intuitive
// qsGraph_halt() and qsGraph_unhalt().  Nothing changes with this
// enter/exit shit added.


// Looks like we get one of these threads per graph, but only if
// qsGraph_enter() is called.
static
void *GraphRunner(struct QsGraph *g) {

    DASSERT(g);
    // This will run requests from thread pool worker threads.
    qsGraph_waitForDestroy(g, 0);

    DSPEW("Graph runner thread EXITING");
    return 0;
}


void qsGraph_launchRunner(struct QsGraph *g) {

    NotWorkerThread();

    DASSERT(g);

    //CHECK(pthread_mutex_lock(&g->mutex));
    CHECK(pthread_mutex_lock(&g->cqMutex));
    
    // This is now the master thread.

    if(!g->haveGraphRunner) {
        CHECK(pthread_create(&g->graphRunnerThread, 0,
                    (void *) GraphRunner, g));
        // This only gets set once, and is never unset.
        g->haveGraphRunner = true;
    }

    //CHECK(pthread_mutex_unlock(&g->mutex));
    CHECK(pthread_mutex_unlock(&g->cqMutex));

    // This is now NOT the master thread.
}
