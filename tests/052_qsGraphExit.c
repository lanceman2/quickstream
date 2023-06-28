#include <stdlib.h>
#include <pthread.h>


#include "../include/quickstream.h"
#include "../lib/debug.h"


// Will using this mutex leak resources that valgrind will complain
// about?
//
// If that changes I really need to know, hence this test exists.
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


int main(void) {


    qsGraph_create(0, 3/*maxThreads*/, "graph 0", "", QS_GRAPH_IS_MASTER);

    CHECK(pthread_mutex_lock(&mutex));

    // We are assuming that no other thread is calling exit() so in that
    // case exit() is thread-safe.  Well anyway we have a mutex lock to
    // test for resource leaks, but in this case, not for making making
    // exit() thread-safe.  Ya, this is testing that we understand shit.
    //
    // The libquickstream destructor will be called to cleanup the
    // quickstream graph; so we hope.
    //
    exit(0);

    // We never get here:
    return 0; // success
}
