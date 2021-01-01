// A terrible CPP macro that sets b to the current block if it is not set
// already.  Also checks that we are in a block declare() call.  Also
// checks that this is the main thread that creates graphs.  Also checks
// that b is a simple block.
//
// Note: blocks can create parameters for other blocks.  That's why
// this macro is important.
//
// Maybe this beats repeating this code many times in this file.
//
// There are 2 cases:
//   1. the creator and owner block are different blocks
//      and they must both be from the same graph.
//   2. the creator and owner block are the same block
//
//   In either case we return the owner block.
//
// The parameter owner block must be a simple block.
//
//


// In declare() or being called from inside the quickstream library.

#define GET_SIMPLEBLOCK_IN_DECLARE(b) \
    do {\
        ASSERT(mainThread == pthread_self(), "Not graph main thread");\
        if(b && b->inWhichCallback != _QS_IN_NONE) {\
            struct QsBlock *creatorB = GetBlock();\
            ASSERT(creatorB->inWhichCallback == _QS_IN_DECLARE,\
                "%s() must be called from declare()", __func__);\
            ASSERT(creatorB == b || creatorB->graph == b->graph);\
        } else if(!b) {\
            b = GetBlock();\
            ASSERT(b->inWhichCallback == _QS_IN_DECLARE,\
                "%s() must be called from declare()", __func__);\
        }\
        ASSERT(b->isSuperBlock == false);\
    } while(0)
