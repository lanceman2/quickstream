
#include "../include/quickstream.h"


int main(void) {


    struct QsGraph *g0 = qsGraph_create(0, 3/*maxThreads*/,
            "graph 0", "", 0);

    qsGraph_create(0, 1/*maxThreads*/, "graph 1", 0, 0);

    qsGraph_create(0, 7/*maxThreads*/, "graph 2", "tp 2", 0);

    qsGraph_destroy(g0);

    return 0; // success
}
