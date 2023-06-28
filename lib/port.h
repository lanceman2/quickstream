
// A port is a job with the addition of connections between them and other
// jobs.  Ports can be connected to other ports; but there are connection
// rules.
//
// The idea of connections between ports is seen by the highest end users;
// where as the jobs in the ports provides the code that runs to transfer
// data between the ports.  The job objects are what the thread pool
// worker threads see.
//
struct QsPort {

    enum QsPortType portType;

    struct QsBlock *block;

    // The "parentBlock" is a history (record) of the last super block (or
    // graph top block) that made (or un-made) a connection with this
    // port.
    //
    // We only need to use this "parentBlock" pointer for input and setter
    // ports.   It's complicated.  It's used in qsGraph_saveSuperBlock().
    //
    // "parentBlock" is the graph or super block that made the last
    // connection with this port.
    //
    // We need this so we may find the connections made by a graph or
    // super block.
    //
    // Note: we need this for control parameters and stream inputs, but we
    // do not need it for stream outputs; because stream inputs only
    // connect to one stream output, so stream inputs alone can with keep
    // a full record all stream connections since an output can only
    // connect to an input.
    //
    // This value will change as more connections are made and un-made as
    // super blocks are loaded recursively, or a graph (top block) makes
    // connections.
    //
    // Think of this as the record of the "last parent block" that made or
    // unmade a connection to this port.  The last one is all that is
    // needed for writing out a super block.  The connections made by
    // child blocks are already done by the time we access this.
    //
    struct QsParentBlock *parentBlock; // graph or super block

    char *name; // port name.
};
