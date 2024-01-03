#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "port.h"
#include "stream.h"
#include "epoll.h"



static inline struct QsStreamJob *Init(int fd, struct QsPort *port,
        struct QsSimpleBlock **b) {


    // The other non-stream port would be for a control parameter
    // setter.
    ASSERT(port == 0, "This code is not written for "
            "non-stream block callbacks yet.");

    struct QsStreamJob *sj = GetStreamJob(CB_DECLARE, b);

    int flags = fcntl(fd, F_GETFL, 0);
    ASSERT(flags != -1);
    ASSERT(-1 != fcntl(fd, F_SETFL, flags | O_NONBLOCK));

ERROR("Write more code here: sj=%p", sj);

    return sj;
}



void qsAddEpollReadJob(int fd, struct QsPort *port) {

    struct QsSimpleBlock *b;
    struct QsStreamJob *sj = Init(fd, port, &b);

ERROR("Write more code here: sj=%p", sj);
}

void qsAddEpollWriteJob(int fd, struct QsPort *port) {

    struct QsSimpleBlock *b;
    struct QsStreamJob *sj = Init(fd, port, &b);

ERROR("Write more code here: sj=%p", sj);
}

