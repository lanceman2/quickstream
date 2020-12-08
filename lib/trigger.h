
struct QsSimpleBlock;
struct QsSignal;


extern
struct QsSignal *sig;



enum QsTriggerKind {

    QsStream,
    QsSignal
};


// TODO: Replace the word trigger with work, job or event???
//
// We trigger work and the job is what we work on with a worker thread
// from the thread pool that the block is assigned to.
//
// Trigger becomes a job and then goes back to being a trigger.
//
// We trigger a job.
//
// From the users perspective a trigger is the cause that makes jobs that
// threads can work on.  It's just a trigger that causes user callback to
// be called by a worker thread from the thread pool.  There can be many
// things that trigger any block to have its' callbacks called, and hence
// there are can be many triggers in a block.
//
struct QsTrigger {

    // A trigger is the potential of a block to have a thread call a
    // block's running/flowing callback, be the callback flow(), flush()
    // or anonymous callback passed in from one of the trigger create
    // functions:  qsTriggerSignalCreate(), ... TODO add to this list...

    // triggers go pop, and make flow/run events.

    // The opaque QsTrigger is a public API interface.

    // This block owns this trigger.
    //
    // Super blocks do not have triggers.
    //
    struct QsSimpleBlock *block;

    // For doubly linked lists of triggers in simple blocks.  This list is
    // in the simple block that owns this trigger.
    struct QsTrigger *next, *prev;
    //
    // The trigger goes from a waiting list to a job queue in the block.
    // This is just a flag to let us know which list the trigger is in.
    bool isInJobQueue;

    // isRunning flag is set at TriggerStart() and unset in TriggerStop():
    //
    // If isRunning is not set the trigger will not make jobs while the
    // stream is running.
    bool isRunning;

    // There are source triggers that are triggers that can be triggered
    // by something that is not another trigger, like the first domino
    // in a line of dominoes.  Triggers are source triggers or not source
    // triggers.  This does not mean that a source trigger can't be
    // triggered by another trigger, like a domino in the middle of a line
    // of dominoes that you knock over with your finger without knocking
    // over any of the up stream dominoes.
    bool isSource;

    // TODO:
    //
    // If this trigger has an action that passes data to a block that is
    // in a different thread pool, then we need MORE INFO LIKE THIS:
    // But if not this stuff will be 0.
    //struct QsThreadPool *otherThreadPool;
    //struct QsSimpleBlock *otherBlock;


    // Size of the struct that inherits this base class struct.
    size_t size;

    // Kind of trigger
    enum QsTriggerKind kind;

    // All triggers need this function to be defined.
    bool (*checkTrigger)(void);

    // callback is called after this trigger pops:
    int (*callback)(void *userData);

    void *userData;
};


struct QsSignal {

    // inherit QsTrigger
    struct QsTrigger trigger;

    // We need a jump variable so we can sometime jump from a signal
    // handler.
    jmp_buf jmpEnv;

    // A flag to mark this as triggered.
    //
    // This type is supposed to be able to be atomically set in a signal
    // handler.
    volatile sig_atomic_t triggered;

    // A flag to show we are about to pause in a system blocking call.
    volatile sig_atomic_t aboutToPause;

    // The signal number.  See run in shell: "kill -L".
    int signum;
};


struct QsStream {

    // inherit QsTrigger
    struct QsTrigger trigger;
};



extern
void TriggerStart(struct QsTrigger *t);


extern
void TriggerStop(struct QsTrigger *t);


extern
void FreeTrigger(struct QsTrigger *t);


extern
bool CheckAndQueueTrigger(struct QsTrigger *t);


// Adding this ifdef can reduce dependences and speed up recompiling,
// by not defining this function unless it's needed.
//
#ifdef define_CallTriggerCallback
//
// We will have a thread pool mutex before calling this and after.  When
// the users trigger callback is called the mutex will be unlocked.
//
// Returns true if the trigger changed its' run/call state and it is a
// source trigger, and false if not.
//
static inline
bool CallTriggerCallback(struct QsTrigger *t, struct QsThreadPool *tp) {


    // TODO: The callback() may need to be called more than once,
    // which is why we made this function.

    CHECK(pthread_mutex_unlock(&tp->mutex));
    // This is the call of a function in a simple block module.
    int ret = t->callback(t->userData);
    CHECK(pthread_mutex_lock(&tp->mutex));

    // The block can change the trigger from this return value, ret.
    if(ret == 0)
        // No change.  Continue to queue up and call the trigger
        // callback.
        return false; // no trigger change.

    if(ret > 0)
        // Remove the trigger callback for the rest of the run/flow
        // cycle.
        //
        TriggerStop(t);
    else
        // ret < 0
        //
        // Remove the trigger from all runs in this graph.  This will
        // destroy the trigger.
        FreeTrigger(t);

    // Return that the trigger was removed from play at least for this
    // run, but only if it's a source trigger.
    return t->isSource;
}
#endif
