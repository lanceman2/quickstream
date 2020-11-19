
struct QsSimpleBlock;


enum QsTriggerKind {

    QsSignal
};




struct QsTrigger {

    // QSTrigger is the public API interface.

    // This block owns this trigger.
    //
    // Super blocks do not have triggers.
    //
    struct QsSimpleBlock *block;

    // For singly linked list of triggers in block.
    struct QsTrigger *next;

    size_t size;

    // Kind of trigger
    enum QsTriggerKind kind;
};


struct QsSignal {

    // inherit QsTrigger
    struct QsTrigger trigger;

    // The signal number.  See run in shell: "kill -L".
    int signum;


};

