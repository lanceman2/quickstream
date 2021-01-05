// 
//
// 1. inline functions that move triggers between different linked lists,
// 
// 2. and move simple blocks between different linked lists because the
//    triggers that the block owns moved to different lists
//


// This function should only be used in this file.
static inline void
_RemoveFromWaitingList(struct QsSimpleBlock *b, struct QsTrigger *t) {

    // remove "t" from the waiting list
    //

    DASSERT(b);
    DASSERT(t);

    if(t->next) {

        t->next->prev = t->prev;
    }

    if(t->prev) {

        DASSERT(b->waiting != t);
        t->prev->next = t->next;

    } else {

        DASSERT(b->waiting == t);
        b->waiting = t->next;
    }
}


// This function should only be used in this file.
static inline
void _FinishQueuingJob(struct QsSimpleBlock *b, struct QsTrigger *t) {

    t->isInJobQueue = true;

    // Since blocks can have more than one trigger this block could have
    // been in its' thread pool job queue already, hence the if().  We
    // also do not queue the block if there is a worker thread busy
    // working on that block.
    if(b->blockInThreadPoolQueue == false && b->busy == false)
        AddBlockToThreadPoolQueue(b);
}


// The triggers have triggered and now there is a job to work on.
//
// This should only be used by a stream trigger.  And there should
// only be one stream trigger per block.
//
// This moves the trigger from the QsSimpleBlock::waiting list to the
// QsSimpleBlock::lastJob list.
//
static inline void
WaitingToLastJob(struct QsTrigger *t) {

    DASSERT(t);
    DASSERT(t->isRunning);
    struct QsSimpleBlock *b = t->block;
    DASSERT(b);

    DASSERT(t->kind >= QsStreamSource,
            "%s() should only be used by stream triggers",
            __func__);

    ////////////////////////////////////////////////////////
    // 0. do nothing if it's in the job queue already
    ////////////////////////////////////////////////////////
    //
    if(t->isInJobQueue) {
        // There should only be one stream trigger per block.
        DASSERT(b->lastJob == t);
        return;
    }

    ////////////////////////////////////////////////////////
    // 1. remove "t" from the waiting list:
    ////////////////////////////////////////////////////////
    //
    _RemoveFromWaitingList(b, t);

    ////////////////////////////////////////////////////////
    // 2. put "t" in job list at lastJob
    ////////////////////////////////////////////////////////
    //
    t->next = 0;
    t->prev = b->lastJob;
    if(b->lastJob) {

        DASSERT(b->firstJob);
        b->lastJob->next = t;
    } else {

        DASSERT(b->firstJob == 0);
        b->firstJob = t;
    }
    b->lastJob = t;

    ////////////////////////////////////////////////////////
    // 3. put "t" and the block in the job queue
    ////////////////////////////////////////////////////////
    //
    _FinishQueuingJob(b, t);
}


// The triggers have triggered and now there is a job to work on.
//
// This moves the trigger from the QsSimpleBlock::waiting list to the
// QsSimpleBlock::firstJob list.
//
static inline void
WaitingToFirstJob(struct QsTrigger *t) {

    DASSERT(t->isRunning);
    struct QsSimpleBlock *b = t->block;
    DASSERT(b);

    ////////////////////////////////////////////////////////
    // 0. do nothing if it's in the job queue already
    ////////////////////////////////////////////////////////
    //
    if(t->isInJobQueue) return;

    ////////////////////////////////////////////////////////
    // 1. remove "t" from the waiting list:
    ////////////////////////////////////////////////////////
    //
    _RemoveFromWaitingList(b, t);

    ////////////////////////////////////////////////////////
    // 2. put "t" in job list at firstJob
    ////////////////////////////////////////////////////////
    //
    t->prev = 0;
    t->next = b->firstJob;

    DASSERT(b->firstJob == 0);

    if(b->firstJob) {

        DASSERT(b->lastJob);
        b->firstJob->prev = t;
    } else {

        DASSERT(b->lastJob == 0);
        b->lastJob = t;
    }
    b->firstJob = t;

    ////////////////////////////////////////////////////////
    // 3. put "t" and the block in the job queue
    ////////////////////////////////////////////////////////
    //
    _FinishQueuingJob(b, t);
}


// This assumes there is a job in the job queue at b->firstJob.
static inline
struct QsTrigger *PopJobBackToTriggers(struct QsSimpleBlock *b) {

    DASSERT(b);
    DASSERT(b->firstJob);

    struct QsTrigger *t = b->firstJob;
    DASSERT(t->block == b);
    DASSERT(t->isInJobQueue);
    DASSERT(t->isRunning);


    ////////////////////////////////////////////////////////
    // 1. remove "t" from the job list:
    ////////////////////////////////////////////////////////
    //
    b->firstJob = t->next;
    if(t->next) {

        DASSERT(t != b->lastJob);
        b->firstJob->prev = 0;
    } else {

        DASSERT(t == b->lastJob);
        b->lastJob = 0;
    }

    ////////////////////////////////////////////////////////
    // 2. put "t" on the waiting list
    ////////////////////////////////////////////////////////
    //
    t->next = b->waiting;
    if(b->waiting)
        b->waiting->prev = t;
    t->prev = 0;
    b->waiting = t;

    ////////////////////////////////////////////////////////
    // 3. mark it as not in the job queue
    ////////////////////////////////////////////////////////
    //
    t->isInJobQueue = false;

    return t;
}
