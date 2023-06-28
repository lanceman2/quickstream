#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "qsGtk_init.h"




atomic_uint qsGtk_finishedRunning = 0;
atomic_uint qsGtk_didRun = 0;


/* This mutex can be locked in all/any thread that calls GTK functions
 * from a non-GTK thread; like for example a quickstream pthread pool
 * worker thread.
 *
 * That will make all running code that accesses any GTK functions never
 * run concurrently and effectively serialize the running of all the GTK
 * code in all threads (pthreads).
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// File descriptors, rfd, and wfd, that are used to synchronize/serialize
// the running of GTK3 main loop threads with non-GTK3 threads that GTK3
// (and glib) know nothing about.
//
static int rfd = -1; // to read with
static int wfd = -1; // to write with
static GSource *source = 0;
static guint sourceId = 0;

static
gboolean prepare(GSource *source, gint *timeout) {

    *timeout = -1; // -1 for infinity
    //*timeout = 1000000 * 100;
    return FALSE;
}


static pthread_mutex_t wr_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;



// Runs in GTK main loop thread as a standard GTK g_source file callback.
//
// We have the mutex lock when this is called.
//
// This dispatch() function is the only place that we can inject running
// code from other threads; that is threads that are foreign to GTK and
// libg.
//
static gboolean dispatch(GSource *source, GSourceFunc callback,
        gpointer user_data) {

    if(qsGtk_finishedRunning)
        return G_SOURCE_REMOVE;

    CHECK(pthread_mutex_lock(&wr_mutex));

    char buf;
    DASSERT(rfd >= 0);
    ssize_t ret;
    // We read one byte.
    ASSERT((ret = read(rfd, &buf, 1)) == 1);
    errno = 0;
    // Good old non-blocking read.
    ASSERT((ret = read(rfd, &buf, 1)) == -1);
    ASSERT(errno == EAGAIN);

    // We let the other threads run by releasing the mutex and
    // waiting on a condition variable.
    CHECK(pthread_cond_wait(&cond, &mutex));

    CHECK(pthread_mutex_unlock(&wr_mutex));

    // Now we know that ReleaseGTKContext() is done waiting and has
    // released mutex.

    return G_SOURCE_CONTINUE;
}


// This structure, source_funcs, becomes part of the g_source object and
// cannot be declared on the stack there the memory would go away after
// the function on the stack is called, so here it is in static storage.
// This will in effect provide an interface for the g_source user to
// change the callbacks on the fly, but I very much doubt that was the
// intent of the GTK3 developers; and I would expect that changing a
// callback will crash your program.  So yet again, bad design by GTK3
// developers.
//
static GSourceFuncs source_funcs = {
    .prepare = prepare,
    .check = 0,
    .dispatch = dispatch,
    .finalize = 0
};



// The qsGetGTKContext() and qsReleaseGTKContext() functions run in
// different non-GTK threads, but are synchronized in how they run with
// GTK threads via a pipe file, 3 mutexes, and a condition variable.
//
// Thread synchronization code at its worst, or is it best.  It may even
// be correct.
//
// This may seem a little overly complicated, but it is what is required
// to be efficient and robust.  The only "well supported" hook that GTK
// provides to sync with its main loop with "other threads" is a g-source
// file callback.


// This cmutex effectively serializes the calling of GetGTKContext() and
// ReleaseGTKContext().
//
// That's a different thing from all the sync-step code between dispatch()
// and GetGTKContext() with ReleaseGTKContext().
//
// We do not know, and should not need to know, how the guts of GTK3 runs
// its' main loop (and its' other threads).  We are just assuming that we
// can call the GTK3 API in a GTK3 users gsource callback (or in another
// thread that is in sync with that); if not GTK3 really sucks.
//
static pthread_mutex_t cmutex = PTHREAD_MUTEX_INITIALIZER;


// Runs in "other" thread to not hose the GTK3 context stuff and shit.
//
// Used by the non-GTK-main-loop threads to request and wait to
// run a function that accesses the GTK API.  For example:
/*
    qsGetGTKContext();
    GtkWidget *w = gtk_label_new("label text");
    //... add to parent widget.
    .
    .
    // finished calling GTK3 API
    qsReleaseGTKContext();

*/

void qsGetGTKContext(void) {

    // So that we only get one context request at a time.
    CHECK(pthread_mutex_lock(&cmutex));

    if(qsGtk_finishedRunning) {
        CHECK(pthread_mutex_lock(&mutex));
        return;
    }

    // Signal the gtk thread by writing one byte to the pipe.  This will
    // trigger the our gsource to have a gsource user event, where-by
    // blocking that GTK thread in a pthread_cond_wait(), so that we can
    // have another non-GTK thread call GTK3 API functions.
    //
    // We are assuming that the "other" GTK3 threads are also waiting on a
    // blocking call, or at least not doing something that would corrupt
    // GTK3 memory, while the gsource user event is blocking (waiting in
    // pthread_cond_wait()), and the non-GTK3 thread is doing GTK3
    // stuff as we tell it to do here in this file.
    DASSERT(wfd != -1);
    ASSERT(write(wfd, "\0", 1) == 1);

    // Wait until the dispatch() function gets to the pthread_cond_wait().
    // If the dispatch() (GTK main loop) thread is calling our 
    // pthread_cond_wait() then we can get this lock.
    //
    CHECK(pthread_mutex_lock(&mutex));
    // Now dispatch() is waiting in pthread_cond_wait(); that should be
    // guaranteed; pthread_cond_wait() is the only place that dispatch()
    // and the GTK threads the releases mutex.

    // We know now that dispatch() is running now because mutex was
    // released by the pthread_cond_wait() call in dispatch().
}


void qsReleaseGTKContext(void) {

    // We now have the mutex lock and the cmutex lock.

    if(qsGtk_finishedRunning) {
        CHECK(pthread_mutex_unlock(&mutex));
        CHECK(pthread_mutex_unlock(&cmutex));
        return;
    }

    // Wake the GTK main loop thread which is waiting in dispatch().
    CHECK(pthread_cond_signal(&cond));

    CHECK(pthread_mutex_unlock(&mutex));
    // Now dispatch() can return by taking its sweet time.

    // Wait until dispatch() gets done running if it is running.
    //
    // We know dispatch() (the insides of it) can't be running if we get
    // this wr_mutex lock.
    CHECK(pthread_mutex_lock(&wr_mutex));
    CHECK(pthread_mutex_unlock(&wr_mutex));
 
    // Now let another call to GetGTKContext() from any "other" thread get
    // this cmutex lock at the top of GetGTKContext().
    CHECK(pthread_mutex_unlock(&cmutex));
}


// Wrapper of gtk_init()
void qsGtk_init(int *argc, char ***argv) {

    // Due to a limitation in GTK3:
    ASSERT(qsGtk_finishedRunning == 0, "This cannot be called more than once");
    ASSERT(qsGtk_didRun == 0);


    errno = 0;

    CHECK(pthread_mutex_lock(&mutex));

    ++qsGtk_didRun;

    gtk_init(argc, argv);

    int fds[2];
    // This pipe is used to synchronize/serialize GTK3 threads with non-GTK3
    // threads.
    ASSERT(pipe(fds) == 0);
    rfd = fds[0];
    wfd = fds[1];
    ASSERT(wfd >= 0);
    ASSERT(rfd >= 0);
    ASSERT(fcntl(rfd, F_SETFL, O_NONBLOCK) == 0);

    source = g_source_new(&source_funcs, sizeof(*source));
    ASSERT(source);
    sourceId = g_source_attach(source, NULL);
    ASSERT(sourceId);
    g_source_add_unix_fd(source, rfd, G_IO_IN);

    // This thread that called this function will have the mutex lock
    // until the g_source event callback releases the mutex in dispatch()
    // which comes from calling gtk_main() and the other gtk main loop
    // functions after now...
}


// There is no cleanup function for gtk_init().  One might say its
// corresponding cleanup (destructor) function is exit().
//
// When this is called there should be no possibility that another thread
// will call qsGetGTKContext() and qsReleaseGTKContext().
//
void qsGtk_cleanup(void) {

    ASSERT(qsGtk_finishedRunning == 0);
    ASSERT(qsGtk_didRun == 1);

    ++qsGtk_finishedRunning;

    g_source_remove(sourceId);
    g_source_destroy(source);

    CHECK(pthread_mutex_unlock(&mutex));

    // This seems to be how GTK cleans up or at least cleans up some
    // things.  This is not documented in GTK3 docs.
    //
    // GTK3 has system resource leaks, but it does seem to cleanup a
    // little.  Why do we do this?  So that we can tell what the hell
    // is going on when debugging this code.
    while(g_main_context_pending(0))
        // We checked, and this is called; so it must be cleaning up
        // something.  I wish GTK3 docs would explain that this is needed
        // and why it is needed.  GTK3 does not have a robust cleanup, and
        // that is by design.  I did piss off the GTK developers when I
        // asked about this.  There is no un-doer to gtk_init().  They
        // just decided that the way to cleanup the system resources from
        // using libgtk*.so and friend libraries was to exit the program.
        // I consider that sloppy coding.  This makes it impossible to
        // make a "straight forward" robust GTK3 quickstream block module.
        // A GTK3 quickstream block can't be cleanly unloaded.  It can be
        // unloaded, but it leaves system resources in your program making
        // the reload not the same as the first loading.
        //
        // Yes of course a smart ass could wrap the offending GTK3
        // libraries with a LD_PRELOAD-like code, but that seems to be
        // more work than benefit repeated.
        //
        // Thankfully, Linux is robust by design (like most all OSes),
        // even when programs running on it are not.
        g_main_context_iteration(0, FALSE);
}
