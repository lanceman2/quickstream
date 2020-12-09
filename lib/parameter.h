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

    void *value;

    // Array of setter or constant parameters in other blocks that this
    // constant parameter connects to.  This is constant at flow-time.
    // Note: these connections can form a loop.
    uint32_t numConnections;
    struct QsParameter **connections;
};


struct QsGetter {

    // We inherit a parameter
    struct QsParameter parameter;

    // Array of setter parameters in other blocks that this getter
    // parameter connects to.  This is constant at flow-time.
    uint32_t numSetters;
    struct QsSetter **setters;
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

    // Setters can be read while the stream is flowing and the value
    // is passed from one block's thread to another block's thread.
    // We lock this mutex when 
    pthread_mutex_t *mutex;

    // This is a flow-time constant.  Passed to the setCallback().
    void *userData;

    // Next setter in the blocks queue of setters to act on.
    //
    // This will change at flow-time.  The block that owns this setter
    // parameter provides a mutex to w/r next and value.
    struct QsSetter *next;

    // This points to allocated memory that is written to before each
    // setcallback() call.  We need the mutex lock to read or write to the
    // memory that this points to.
    void *value;

    void (*setCallback)(struct QsParameter *p,
            const void *value, size_t size, void *userData);

    // Set if the feeder parameter is constant.
    //
    // Feeders can be either constant or getters.
    //bool feederIsConstant;
};
