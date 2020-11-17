#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static int count = 0;


int bootstrap(struct QsGraph *graph) {

    DSPEW("count = %d", count++);


    return 0; // 0 => success
}

int construct(void) {

    return 0; // success 
}

void destroy() {

    DSPEW();
}
