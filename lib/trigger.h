
struct QsSimpleBlock;
struct QsSignal;


extern
struct QsSignal *sig;



enum QsTriggerKind {

    QsStream,
    QsSignal
};


// TODO: Replace the word trigger with work or job???
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
    // block's running callback, be the callback flow(), flush() or
    // anonymous callback passed in from one of the trigger create
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
    bool isRunning;

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
