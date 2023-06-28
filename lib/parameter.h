
// Control parameter
struct QsParameter {

    struct QsPort port; // inherit port

    // Points to a value if there is one yet.
    //
    // If the parameter is not connected to a group than this
    // may point to a malloc() allocated value.
    //
    // If the parameter is connected to a group than this points
    // to the next value to be read in the group ring buffer.
    void *value;

    struct QsGroup *group;

    enum QsValueType valueType;

    size_t size;
};


// The getter control parameter has no job.  It is a source of data that
// is activated by the block code calling qsGetterPush() (or whatever it's
// called).  It pushes values to the connected setter parameters.
struct QsGetter {

    struct QsParameter parameter; // inherit parameter
};


struct QsSetter {

    // The API users will get a pointer to parameter.
    //
    struct QsParameter parameter; // inherit parameter

    // We'll use offsetof() to get to the top of this struct from job.
    //
    // The API users never get a pointer to job.
    //
    // We use the job peers to keep the list of parameter connections.
    //
    struct QsJob job;

    // This counter, readCount, is compared to the group writeCount to
    // determine what values can be read by the setter callback().
    //
    uint32_t readCount;

    // This could be 0; where the user (block writer) could poll the
    // current value, and not have a call-back function; note, that would
    // make the above job useless.
    int (*callback)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t writeCount, void *userData);
};


// When parameters connect they form a parameter connection group.  Values
// of the "control parameters" are shared between the connected parameters
// using this group data structure.
//
struct QsGroup {

    // Multi-threaded access control mutex.
    //
    // Used as the single job mutex in qsJob_lock() and qsJob_unlock().
    //
    pthread_mutex_t mutex;


    // The list (as an array) of all setter jobs pointers in the group.
    struct QsJob **sharedPeers;


    // There can be only be 0 or one getter in a group.
    struct QsGetter *getter;


    // This is the same size as in all the parameters in this group.
    //
    // This is redundant but getting it's value from the jobs in
    // sharedPeers is a littler cumbersome.
    //
    size_t size;


    // writeCount is a counter used to get values from the ring buffer.
    //
    // The setter parameters that get values compare the setter writeCount
    // to the group writeCount in order to determine what values to access
    // in the ring buffer.
    //
    // Using uint64_t is not necessary.  We handle the wrapping anyway and
    // we know we could wrap uin64_t through 0 too.  Programmers need to
    // not be stupid about thinking that large values wrapping through 0.
    // It's the difference in counter values that matter and not absolute
    // values.
    //
    uint32_t writeCount;

    // The number of values stored in the ring buffer before it wraps back
    // over itself.  Is the array size of values[] below.
    //
    // Not to confused with the stream ring buffer.
    //
    uint32_t numValues;

    // Points to the next value that will be written in the ring
    // buffer/queue.
    //
    // value will be zero, 0, until we get a value set.
    //
    // So value is both, a flag that we have a value ever, and the next
    // value when we have any values.
    //
    // This fixes the case when writeCount wraps through 0; because now
    // we can tell the difference between: starting with no values, and
    // writeCount wrapping through 0 as a value is added.
    //
    void *value;

    // ring buffer/queue of values.  Pointer to a malloc() allocated array
    // with elements of size = leader->size, the parameter size.
    //
    // Not to be confused with the stream ring buffer; a hell of a lot
    // smaller buffer size.
    //
    // Gets allocated when the group is formed, whither there is a value
    // set or not.
    //
    void *values;

    // The number of parameters in the group including the getter if there
    // is one.
    uint32_t numParameters;

};


extern
int qsParameter_connect(struct QsParameter *p1, struct QsParameter *p2);


extern
bool qsParameter_canConnect(struct QsParameter *p1,
        struct QsParameter *p2, bool verbose);
