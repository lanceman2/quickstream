
struct QsDictionary;
struct QsGetter;



struct QsBlock {

    // lists of parameters owned by this block
    struct QsDictionary *getters;
    struct QsDictionary *setters;


    // We queue up setCallbacks that need to be called in the owner block
    // thread before and/or after the work() call.  This will queue up
    // many different parameters, but only the last element value is
    // queued for a given parameter, so each parameter can only have one
    // entry in this queue.
    //
    // This queue is added to by calls to qsParameterGetterPush() calls
    // from this or other blocks
    //
    // We do not expect a lot of inter-thread contention for this queue,
    // but we do need a mutex lock to access this queue.
    pthread_mutex_t mutex;

    // pointers to the setter parameter callbacks that are queued.
    // This block owns these setter parameters.
    struct QsSetter *first, *last;
};
