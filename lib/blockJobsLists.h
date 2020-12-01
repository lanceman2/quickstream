#ifndef _QS_IN_BOOTSTRAP
#  error "block.h must be included before blockJobsLists.h"
#endif


// This function should only be used in this file.
static inline void
_RemoveFromTriggersList(struct QsSimpleBlock *b, struct QsTrigger *t) {

    // remove "t" from the triggers list
    //

    DASSERT(b);
    DASSERT(t);

    if(t->next) {

        DASSERT(b->lastJob != t);
        t->next->prev = t->prev;

    } else {

        DASSERT(b->lastJob == t);
        b->lastJob = t->prev;
    }

    if(t->prev) {

        DASSERT(b->firstJob != t);
        t->prev->next = t->next;

    } else {

        DASSERT(b->firstJob == t);
        b->firstJob = t->next;
    }
}


// The triggers have triggered and now there is a job to work on.
//
// This moves the trigger from the QsSimpleBlock::triggers list to the
// QsSimpleBlock::lastJob list.
//
static inline void
TriggersToLastJob(struct QsSimpleBlock *b, struct QsTrigger *t) {


    ////////////////////////////////////////////////////////
    // 1. remove "t" from the triggers list:
    ////////////////////////////////////////////////////////
    //
    _RemoveFromTriggersList(b, t);

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
}


// The triggers have triggered and now there is a job to work on.
//
// This moves the trigger from the QsSimpleBlock::triggers list to the
// QsSimpleBlock::firstJob list.
//
static inline void
TriggersToFirstJob(struct QsSimpleBlock *b, struct QsTrigger *t) {


    ////////////////////////////////////////////////////////
    // 1. remove "t" from the triggers list:
    ////////////////////////////////////////////////////////
    //
    _RemoveFromTriggersList(b, t);

    ////////////////////////////////////////////////////////
    // 2. put "t" in job list at firstJob
    ////////////////////////////////////////////////////////
    //
    t->prev = 0;
    t->next = b->firstJob;
    if(b->firstJob) {

        DASSERT(b->lastJob);
        b->firstJob->prev = t;
    } else {

        DASSERT(b->lastJob == 0);
        b->lastJob = t;
    }
    b->firstJob = t;
}


// This assumes there is a job in the job queue at b->firstJob.
static inline
struct QsTrigger *PopJobBackToTriggers(struct QsSimpleBlock *b) {

    DASSERT(b);
    DASSERT(b->firstJob);

    struct QsTrigger *t = b->firstJob;

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
    // 2. put "t" on the triggers list
    ////////////////////////////////////////////////////////
    //
    t->next = b->triggers;
    if(b->triggers)
        b->triggers->prev = t;
    t->prev = 0;
    b->triggers = t;


    return t;
}
