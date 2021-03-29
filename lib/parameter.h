struct QsParameter;
struct QsSetter;
struct QsGetter;
struct QsConstant;
struct QsBlock;



struct QsParameter {

    // The block that owns this parameter.  Super blocks do not own
    // parameters.
    struct QsSimpleBlock *block;

    // name of the parameter.  Unique for each block for all setters
    // and unique for each block for all getters.  strdup() memory.
    const char *name;

    // This points to memory that is the size of parameter.  When
    // connected to other parameters it sometimes points to the same
    // memory as another parameter, depending on the kind of parameter and
    // how it's connected.
    void *value;

    // Lots of little shit details here, i.e. the nature of the beast is
    // complex, and there's no way around it.
    //
    // There are two kinds of lists of connections or connection groups:
    //
    //   1. Getter --> N Setters
    //        In a group they all have the same list and the Getter is
    //        "first" and they all have the same "numConnections".  There
    //        must be one Getter in the group.
    //        The "value" is not shared, and data must be copied to the
    //        setters from the getter and between thread blocks.
    //        Requires mutex locks to do the copies.
    //
    //   2. Constants and Setters in a group
    //        and that includes Constant  --> Setters
    //        and               Constant <--> Constant
    //        and                 Setter <--> Setters
    //        in a fully connected topology.
    //        In a group they all have the same "first" and
    //        "numConnections".
    //        We require the convention that the "first" must be a
    //        constant or setter parameter.
    //        There can be 0,1,2,3,... constants in the group.
    //        There can be 0,1,2,3,... setters in the group.
    //
    //
    // The "first" and "numConnections" are repeated in every element
    // parameter in the list.  "next" points to the next parameter in the
    // list.  We tried other structures, but this seemed the simplest, and
    // requires no memory allocations (beyond the parameter) to connect
    // and disconnect any parameters, except "value" above.
    //
    // "next" keeps a singly linked list with each element having a
    // pointer to the "first" and the count "numConnections".
    //
    // "numConnections" is more like the number of nodes in the connection
    // group.
    //
    struct QsParameter *first; // first in the list
    struct QsParameter *next;  // next in the list
    uint32_t numConnections;   // number in the list


    // size in bytes of the parameter data that is copied.  If the value
    // passed is a double array[5], then size = 5*sizeof(double).
    //
    size_t size;

    // Kind of parameter: QsConstant, QsGetter, or QsSetter
    enum QsParameterKind kind;

    // double, float, uint64_t and structures we can make up.
    //
    // This should align well with C++ template types.  The C++ template
    // user API to the Parameters will be better than the C API.
    //
    enum QsParameterType type;
};


struct QsGetter {

    // Getters only connect to Setters in one group, with one getter.

    // We inherit a parameter
    struct QsParameter parameter;

    // If set, value, is the initial value from qsParameterGetterCreate() call
    // before the graph runs.  We push it to Setters at the start of the
    // graph run.
    //
};


struct QsConstant {

    // We inherit a parameter
    struct QsParameter parameter;

    // Current value.  This pointer points to memory that is shared by all
    // connected parameters.
    // void *value; // in parameter

    // Called each time the value changes.  The value can only change
    // while the stream flow is paused, so no mutex is needed.
    int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData);

    void *userData;
};


struct QsSetter {

    // We inherit a parameter.
    struct QsParameter parameter;

    // This trigger will be owned by the block that owns this parameter.
    // This trigger is not created unless this setter is connected to from
    // a getter parameter.
    struct QsTrigger *trigger;

    // Setters can be read while the stream is flowing and the value is
    // passed from one block's thread to another block's thread.  We use
    // this mutex when access value, and this is not connected to a
    // constant parameter.
    //
    // mutex is set, pointing to the owner block mutex, if the feeder is a
    // getter parameter.  mutex is 0 if feeders are constant parameters.
    pthread_mutex_t *mutex;

    // We need a mutex lock to access value, when not connected to a
    // constant parameter.
    //
    // The current queued up value to be passed to setCallback() next
    // time.
    //void *value; // in parameter

    // This is a user definable constant.  Passed to the setCallback().
    void *userData;

    int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData);


    // Flag that says we have a value loaded and it has not been read yet
    // and the trigger is queued.
    bool haveValueQueued;
};


extern
void GettersStart(struct QsGraph *g);


extern
void QueueUpSetterFromGetter(struct QsSetter *s,
        struct QsParameter *getter);
