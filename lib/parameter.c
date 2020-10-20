#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "block.h"
#include "../include/quickstream/block.h"


enum QsParameterKind {

    QsGetter,
    QsSetter,
    QsConstant
};

struct QsParameter {

    // This struct is constant after it is created.

    // The block that owns this parameter.
    struct QsBlock *block;

    // name of the parameter.  Unique for each block for all setters
    // and unique for each block for all getters.
    const char *name;

    // size in bytes of the parameter data that is copied for each 
    size_t size;

    // data type
    enum QsParameterType type;

    // Bit-wise or-ed flag
    //
    // QS_KEEP_AT_RESTART (02)
    // QS_FREE_USERDATA   (04)
    // QS_Q_BEFORE_WORK   (010)
    // QS_Q_AFTER_WORK    (020)
    //
    uint32_t flags;


    // Kind of parameter
    enum QsParameterKind kind;
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

    // TODO: Is this needed:
    //
    // Zero or one getter or constant may connect to this setter.
    //struct QsParameter *feeder;


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
    const void *value;

    void (*setCallback)(struct QsSetter *p,
            const void *value, size_t size, void *userData);

    // Set if the feeder parameter is constant.
    bool isConstant;
};
