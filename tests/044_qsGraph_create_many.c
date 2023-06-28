
#include "../include/quickstream.h"


int main(void) {

    struct QsGraph *g0 = qsGraph_create(0, 3/*maxThreads*/,
            "graph 0", "", 0);

    struct QsGraph *g1 = qsGraph_create(0, 1/*maxThreads*/,
            "graph 1", 0, QS_GRAPH_IS_MASTER);

    struct QsGraph *g2 = qsGraph_create(0, 7/*maxThreads*/,
            "graph 2", "tp 2", QS_GRAPH_IS_MASTER);

    qsGraph_destroy(g0);
    qsGraph_destroy(g1);
    qsGraph_destroy(g2);

    return 0; // success
}
