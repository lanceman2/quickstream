// It seems that signals do not mix easily with other events.
//
// This injects events/jobs from a "signal thread" into the worker thread
// pools events/jobs.  It's just a single very sleepy thread that feeds
// the "regular" quickstream thread pool worker threads.
//
// Since signals are a process wide thing; we make this signal thread
// thing function across all blocks in all graphs.  Putting this kind of
// thing in the thread pool worker threads would at least double the
// complexity of the thread pool worker threads main loop.  We are not
// seeing great demand for this, so we are keeping it more separate from
// the general thread pool worker threads.  We envision that this code is
// robust and as simple as we could make it.
//

#define _QS_WAKEUP_SIG  (SIGCONT)



struct QsSignalJob {

    // inherit job.
    struct QsJob job;

    int (*callback)(void *userData);

    void *userData;

    // Points back to the struct QsSignal.
    struct QsSignal *signal;
};


// a container for an array of jobs:
//
struct QsSignal {

    // We keep an array of signal job pointers.  There can be any number
    // of blocks using a given signal number.
    //
    struct QsSignalJob **signalJobs;
    //
    // Length of array of signal jobs.
    //
    uint32_t numSignalJobs;

    // See signal number from running "kill -L"
    //
    int sigNum; // signal number like for example: SIGUSR2

    // For the list of jobs in the signal thread thingy.
    CRBNode node;
};



// The signal thread will not exist unless it is needed.  We let it handle
// any number of signals.  We remove it if it is not needed after being
// used.  Instead of making a static singleton object we make it (malloc
// it) dynamically when it is needed saving memory when it is not
// needed.
//
struct QsSignalThread {

    pthread_t pthread;

    pthread_barrier_t *barrier;

    sigset_t sigset;

    // This red black tree list is keyed by signal number.
    //
    CRBTree signals;

    // Flag that says the signal thread is finished.
    atomic_uint done;
};
