
#include "../include/quickstream.h"


int main(void) {

    struct QsGraph *g = qsGraph_create(0, 3/*maxThreads*/,
            "graph 1", "tp 1", 0);

    qsGraph_destroy(g);

    return 0; // success
}
