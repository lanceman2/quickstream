struct QsParameter;
struct QsSetter;
struct QsGetter;
struct QsConstant;
struct QsBlock;



struct QsParameter {

    // This struct is constant after it is created.

    // The block that owns this parameter.  Super blocks do not
    // own parameters.
    struct QsSimpleBlock *block;

    // name of the parameter.  Unique for each block for all setters
    // and unique for each block for all getters.
    const char *name;

    // size in bytes of the parameter data that is copied for each.
    // If the value passed is a double array[5]; then size =
    // 5*sizeof(double) or sizeof(array).
    //
    size_t size;

    // Bit-wise or-ed flag
    //
    // TODO: This stupid list is evolving.
    //
    // QS_FREE_USERDATA   (01)
    //
    uint32_t flags;

    // Kind of parameter: QsConstant, QsGetter, or QsSetter
    enum QsParameterKind kind;

    // double, float, uint64_t and shit like that.
    enum QsParameterType type;

};


struct QsConstant {

    // We inherit a parameter
    struct QsParameter parameter;

    // Current value.  This pointer points to memory that is shared by all
    // connected parameters.
    void *value;

    // Called each time the value changes.  The value can only change
    // while the stream flow is paused, so no mutex is needed.
    void (*setCallback)(struct QsParameter *p,
            void *value, size_t size, void *userData);

    // Array of constant parameters in all blocks that this constant
    // parameter connects to.  Constant parameters form a fully connected
    // topological graph with all constants parameters that they connect
    // with.  Some connections may be setter parameters.
    //
    // connections points to an array that is shared by all connections
    // that connect to this parameter.  This starts with array size 0 and
    // jumps to 2 when the first connection happens so that it includes a
    // pointer for this constant and the other parameter that it connects
    // to.
    struct QsParameter **connections;
    //
    // numConnections is the array size of the connections pointers.  We
    // end up will many copies of this number (with the same value), one
    // in each constant parameter that this constant parameter connect
    // to.  
    uint32_t numConnections;
};


struct QsGetter {

    // We inherit a parameter
    struct QsParameter parameter;

    // If set is the initial value from qsParameterGetterCreate() call
    // before the graph runs.
    // We push it to Setters at the start of the graph run.
    //
    // We store the last value that was sent, which can be retrieved with
    // qsParameterGetValue().
    void *value;

    // Array of setter parameters in other blocks that this getter
    // parameter connects to.  This is constant at flow-time.
    struct QsSetter **setters;
    uint32_t numSetters;
};


struct QsSetter {

    // We inherit a parameter.
    struct QsParameter parameter;

    // This trigger will be owned by the block that owns this parameter.
    // This trigger is not created unless this setter is connected to from
    // a getter parameter.
    struct QsTrigger *trigger;

    // TODO: Is this needed:
    //
    // Zero or one getter or constant may connect to this setter.
    //
    // 0 if not connected.  It's a flag too.
    //
    struct QsParameter *feeder;

    // Setters can be read while the stream is flowing and the value is
    // passed from one block's thread to another block's thread.  We use
    // this mutex when access value, and this is not connected to a
    // constant parameter.
    //
    // mutex is set, pointing to the owner block mutex, if the feeder is a
    // getter parameter.  mutex is 0 if feeders are constant parameters.
    pthread_mutex_t *mutex;

    // We need a mutex lock to access value, when no connected to a
    // constant parameter.
    //
    // The current queued up value to be passed to setCallback() next
    // time.
    void *value;

    // This is a flow-time constant.  Passed to the setCallback().
    void *userData;

    int (*setCallback)(struct QsParameter *p,
            void *value, size_t size, void *userData);


    // Flag that says we have a value loaded and it has not been read yet
    // and the trigger is queued.
    bool haveValueQueued;

    // Set if the feeder parameter can be constant or getter.
    //
    // This is a block writer option.
    //
    bool callbackWhilePaused;
};


extern
void GettersStart(struct QsGraph *g);
