
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

    // For singly linked list of triggers in block.
    struct QsTrigger *next;

    // Size of the struct that inherits this base class struct.
    size_t size;

    // Kind of trigger
    enum QsTriggerKind kind;
};


struct QsSignal {

    // inherit QsTrigger
    struct QsTrigger trigger;

    void (*triggerCallback)(void *userData);

    void *userData;

    // The signal number.  See run in shell: "kill -L".
    int signum;

};

