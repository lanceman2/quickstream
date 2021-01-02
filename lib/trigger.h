
struct QsSimpleBlock;
struct QsSignal;


extern
struct QsSignal *sig;



enum QsTriggerKind {

    QsSetterTrigger, // for calling setter parameter callback
    QsSignal,   // OS signal trigger callback

    // Stream triggers go after all others.  So we can tell they are for
    // streams by the greater value

    // QsStreamSource is a source trigger that needs to be queued any time
    // that the output is not clogged and flow() is happy.
    //
    QsStreamSource, // QsStreamSource must be first of streams triggers.

    // QsStreamIO is a trigger that is queued any time there is enough
    // input and none of the outputs are clogged and flow() is happy.
    QsStreamIO
};


// TODO: Replace the word trigger with work, job or event???
//
// Not likely to happen, but we keep this note so developers can get an
// idea of what the hell a trigger is.  The English language does not have
// a word for a trigger-event-to-job-work-thingy.  It's all those things
// and it starts with a trigger.
//
// We trigger work and the job is what we work on with a worker thread
// from the thread pool that the block is assigned to.
//
// Trigger becomes a job and then goes back to being a trigger again and
// waits for another triggered event which queues up a job and bla bla
// bla.
//
// We trigger a job.
//
// From the users perspective a trigger is the cause that makes jobs that
// threads can work on.  It's just a trigger that causes user callback to
// be called by a worker thread from the thread pool.  There can be many
// things that trigger any block to have its' callbacks called, and hence
// there can be many triggers in a block.
//
struct QsTrigger {

    // A trigger is the potential of a block to have a thread call a
    // block's running/flowing callback, be the callback flow(), flush()
    // or anonymous callback passed in from one of the trigger create
    // functions:  qsTriggerSignalCreate(), ... TODO add to this list...

    // triggers go pop, and make flow/run events.

    // The opaque QsTrigger is a public API interface.

    // This block owns this trigger.  A block can own many triggers.
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
    // If isRunning is not set the trigger will not make jobs while
    // quickstream is running.
    bool isRunning;

    // There are source triggers that are triggers that can be triggered
    // by something that is not another trigger, like the first domino
    // in a line of dominoes.  Triggers are source triggers or not source
    // triggers.  This does not mean that a source trigger can't be
    // triggered by another trigger, like a domino in the middle of a line
    // of dominoes that you knock over with your finger without knocking
    // over any of the up stream dominoes, and the down stream dominoes
    // fall.
    //
    // If there are no source triggers in a graph than running the graph
    // will start and then quickly stop.  The running between start and
    // stop will be quick, and do near nothing.  No worker threads will
    // get launched is there are no source triggers in the graph.
    bool isSource;

    // This is only a quickstream internal flag, that marks that we keep
    // the threadPool mutex lock while calling the callback() of this
    // struct.  Clearly API users can't set this flag.
    bool keepLockInCallback;

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
    bool (*checkTrigger)(void *userData);

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

    // This gets set to the thread that has set the aboutToPause flags.
    // This thread will be the thread that is waiting and can jump to
    // try to take the action for this signal trigger.
    pthread_t thread;

    // The signal number.  See a list run in shell: "kill -L".
    int signum;
};


struct QsStreamIO {

    // A trigger that is triggered by just the stream input and output.

    // inherit QsTrigger
    struct QsTrigger trigger;
};


struct QsStreamSource {

    // A trigger that is letting the worker thread calling a system
    // read(2).  Could be a fread(3) which wraps read(2) and stuff like
    // that.

    // inherit QsTrigger
    struct QsTrigger trigger;
};


struct QsStreamFdRead {

    // A trigger based on epoll_wait(2) for a reading fd.

    // inherit QsTrigger
    struct QsTrigger trigger;

    int fd;
};


struct QsStreamFdWrite {

    // A trigger based on epoll_wait(2) for a writing fd.

    // inherit QsTrigger
    struct QsTrigger trigger;

    int fd;
};



extern
void TriggerStart(struct QsTrigger *t);


extern
void TriggerStop(struct QsTrigger *t);


extern
void FreeTrigger(struct QsTrigger *t);


extern
bool CheckAndQueueTrigger(struct QsTrigger *t);

extern
void *AllocateTrigger(size_t size, struct QsSimpleBlock *b,
        enum QsTriggerKind kind, int (*callback)(void *userData),
        void *userData, bool (*checkTrigger)(void *userData),
        bool isSource);
