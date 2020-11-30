
struct QsSimpleBlock;


enum QsTriggerKind {

    QsSignal // TODO: add more kinds
};




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

    // For singly linked list of triggers in simple blocks.
    struct QsTrigger *next;

    // Size of the struct that inherits this base class struct.
    size_t size;

    // Kind of trigger
    enum QsTriggerKind kind;

    // All triggers need this function to be defined.
    bool (*checkTrigger)(void);
};


struct QsSignal {

    // inherit QsTrigger
    struct QsTrigger trigger;

    // This is call when this trigger pops:
    void (*triggerCallback)(void *userData);

    // A flag to mark this as triggered.
    //
    // This type is supposed to be able to be atomically set and read.
    volatile sig_atomic_t triggered;

    void *userData;

    // The signal number.  See run in shell: "kill -L".
    int signum;
};



extern
void TriggerStart(struct QsTrigger *t);


extern
void TriggerStop(struct QsTrigger *t);
