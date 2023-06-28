// Testing making a block and a job.
//
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/Dictionary.h"

#include "../lib/c-rbtree.h"
#include "../lib/name.h"
#include "../lib/threadPool.h"
#include "../lib/block.h"
#include "../lib/graph.h"
#include "../lib/job.h"


static
bool Work(struct QsJob *j) {

    DSPEW();

    return false;
}

void Catcher(int sig) {
    ERROR("Caught signal %d", sig);
    ASSERT(0);
}


int main(void) {

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);


    struct QsGraph *g = qsGraph_create(0, 3, 0, 0, 0);

    struct QsJobsBlock b;

    qsBlock_init(g, &b.block,
            0/*parent block*/,
            0/*thread pool*/,
            QsBlockType_jobs, 0/*name*/, 0);

    struct QsJob j;

    qsJob_init(&j, &b, Work, 0, 0);

    qsJob_cleanup(&j);

    qsBlock_cleanup(&b.block);

    qsGraph_destroy(g);

    return 0;
}
