#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/Dictionary.h"
#include "../lib/name.h"

#include "../lib/c-rbtree.h"
#include "../lib/threadPool.h"
#include "../lib/block.h"
#include "../lib/graph.h"
#include "../lib/job.h"


void catcher(int signum) {
    ASSERT(0, "Catch signal %d", signum);
}



int main(void) {

    ASSERT(0 == signal(SIGSEGV, catcher));
    ASSERT(0 == signal(SIGABRT, catcher));

    struct QsGraph *g = qsGraph_create(0, 3/*maxThreads*/, "graph 0", "", 0);
    ASSERT(g);

    // Do not wait very long.  Test that it does not hang forever.
    qsGraph_wait(g, 0.0001/*seconds*/);

    return 0; // success
}
