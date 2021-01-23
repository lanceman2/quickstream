
// We have a threadPool mutex lock when this is called.
//
// Returns true if block->flow() is callable.
//
static inline bool
CheckBlockFlowCallable(struct QsSimpleBlock *b) {

    DASSERT(b->streamTrigger);

    if(b->callingFlow || !b->streamTrigger->isRunning)
        // This filter block has a worker thread actively calling flow(),
        // so there is no need to queue a worker for it.  It will just
        // keep on working until working or queuing is not necessary, so
        // we don't and can't queue it now.
        return false;


    // If any outputs are clogged than we cannot call flow().
    for(uint32_t i = b->numOutputs-1; i != -1; --i) {
        struct QsOutput *output = b->outputs + i;
        for(uint32_t j = output->numInputs-1; j != -1; --j)
            if(output->inputs[j]->readLength >= output->maxLength) {
                // TODO: remove this DASSERT().
                //
                // We have a clog therefore we better have a thread
                // job or a thread working:
                DASSERT((b->threadPool->maxThreads == 0 &&
                            b->threadPool->first) ||
                        b->block.graph->numWorkingThreads > 
                        b->block.graph->numIdleThreads, 
                        "block %s has clogged output",
                        b->block.name);
                // We have at least one clogged output reader.  It has a
                // full amount that it can read.  And so we will not be
                // able to call flow().  Otherwise we could overrun the
                // read pointer with the write pointer.
                return false;
            }
    }

    // If any input meets the threshold we can call flow() for this filter
    // block, b, we return true.  If this filter block, b, decides that
    // this one simple threshold condition is not enough then that filter
    // block's flow() call can just do nothing and than this will try
    // again later when another feeding filter block returns from a flow()
    // call and we do this again.
    for(uint32_t i = b->numInputs-1; i != -1; --i) {
        struct QsInput *input = b->inputs + i;
        // TODO: we must call the flow() of flush() if this is flushing,
        // even if we are not at the threshold.
        //if(input->readLength >= input->threshold)
        if(input->readLength)
            return true;
    }

    if(b->numInputs == 0)
        // We have no inputs so the inputs are not a restriction.
        return true;

    return false;
}
