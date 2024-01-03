// This simple built-in block is special and that creates issues.
//
// This is a quickstream built-in block which receives epoll read and
// write file descriptor (epoll) polling requests from libquickstream.so
// simple block declare() interface functions qsAddEpollReadJob() and
// qsAddEpollWriteJob().

// This block can only get loaded once per graph.

// TODO: Other blocks can call qsAddEpollReadJob() and
// qsAddEpollWriteJob(), and those blocks consequently depend on this
// block being loaded in the graph.  There may be a way to automatically
// load this block by calls from qsAddEpollReadJob() and
// qsAddEpollWriteJob() from any other block without "breaking"
// quickstreamGUI.  Maybe make simple blocks that are not "seen" by the
// graph builder code.
//

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>

#include "../../../../include/quickstream.h"

#include "../../../debug.h"
#include "../../../Dictionary.h"

#include "../../../c-rbtree.h"
#include "../../../name.h"
#include "../../../threadPool.h"
#include "../../../block.h"
#include "../../../graph.h"
#include "../../../job.h"
#include "../../../port.h"
#include "../../../stream.h"
#include "../../../epoll.h"



int Epoll_declare(void) {

    // Blocks normally do not do this:
    struct QsBlock *b = GetBlock(CB_DECLARE, 0, QsBlockType_simple);

// Remove compiler error for now.
ERROR("b=%p", b);

    struct Epoll *epoll = calloc(1, sizeof(*epoll));
    ASSERT(epoll, "calloc(1, %zu) failed", sizeof(*epoll));

    qsSetUserData(epoll);

ERROR("This block is not ready for use yet");
ERROR("I'm thinking now that the epoll service should be a "
        "hidden block that is of block type QsBlockType_job"
        " See QsBlockType_job in ../include/block.h");

              // return 0 ==> success
    return 1; // return != 0 ==> Fail to load for now.
              // return 1 ==> fail but call undeclare().
              // return -1 ==> fail and do not call undeclare().
}


int Epoll_undeclare(struct Epoll *epoll) {

    DSPEW();
    DASSERT(epoll);
    free(epoll);

    return 0;
}

