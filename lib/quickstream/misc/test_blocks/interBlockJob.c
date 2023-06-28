#include "pthread.h"

#include "../../../../include/quickstream.h"
#include "../../../debug.h"

#include "./interBlockJob_base.h"

static
struct Base *base = 0;

static
struct QsInterBlockJob *qj = 0;

static
void callback(double *x) {

    DSPEW("   Block \"%s\"   got value=%lg", qsBlockGetName(), *x);

    *x += 1.0;

    if(*x > 70)
        // Stop queuing up jobs.
        return;

    if(!qj) {
        CHECK(pthread_mutex_lock(&base->mutex));
        qj = base->jobs[base->numJobs - 1];
        CHECK(pthread_mutex_unlock(&base->mutex));
    }

    // Keep it going by queuing another job.
    qsQueueInterBlockJob(qj, x);
}


static void *dlhandle = 0;


int declare(void) {

    DSPEW();

    struct QsInterBlockJob *j =
        qsAddInterBlockJob((void (*)(void *)) callback, sizeof(double), 3);

    // Add a shared object library that is in this directory.
    dlhandle = qsOpenRelativeDLHandle("_interBlockJob_base.so");

    // Get this data from the shared object library.
    base = *(void **) qsGetDLSymbol(dlhandle, "base");

    CHECK(pthread_mutex_lock(&base->mutex));

    if(base->numJobs == 1) {
        double val = 0.0;
        qsQueueInterBlockJob(qj = base->jobs[0], &val); 
    } else if(base->numJobs)
        qj = base->jobs[base->numJobs - 1];

    // Call the added function, passing it data from this block.
    base->AddJob(j);

    CHECK(pthread_mutex_unlock(&base->mutex));


    return 0;
}


int undeclare(void *userData) {

    // TODO: This dlclose() wrapper call (qsCloseDLHandle) may be
    // unnecessary given that this block DSO is unloaded via dlclose()
    // which seems to keep track of DSO dependencies.  The man page does
    // not explicitly talk about this: Does a DSO (via dlopen()) that
    // loads another DSO automatically unload that requested DSO when
    // the requester DSO is unloaded?  I'm just not going to worry about
    // calling this, even if the cleanup of the requested DSOs is
    // automatic.  This should not hurt.
    //
    // Also: we could have had libquickstream.so keep a tally of dlhandles
    // and have them automatically closed when the block is unloaded; but
    // it's not to big a deal given that the block needs to keep a copy of
    // the dlhandle to get symbols with anyway.  Counter to that is
    // that, we could maybe skip defining an undeclare() function had
    // libquickstream.so provided automatic cleanup of dlhandles.
    //
    qsCloseDLHandle(dlhandle);

    return 0;
}
