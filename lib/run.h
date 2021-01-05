// Returns 0 if it runs and 1 if not.
//
extern
int run(struct QsGraph *graph);


extern 
void *runWorker(struct QsThreadPool *tp);


// We need the graph/threadPool mutex locked to call this.
extern
void LaunchWorkerThread(struct QsThreadPool *tp);


// We need the graph/threadPool mutex locked to call this.
static inline
void CheckMakeWorkerThreads(struct QsThreadPool *tp) {

    if(tp->first == 0)
        // There are no blocks in the job queue.
        return;

    // We have more than one block to work on.
    if(tp->numIdleThreads)
        // wake an idle thread.
        //
        // man pthread_cond_signal says: If several threads are
        // waiting on cond, exactly one is restarted, but it is
        // not specified which.  So this should wake just one,
        // very cool.
        CHECK(pthread_cond_signal(&tp->cond));
        //
    else if(tp->numThreads < tp->maxThreads)
        // Launch another worker thread.
        LaunchWorkerThread(tp);
}
