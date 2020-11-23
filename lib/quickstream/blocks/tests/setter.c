#include <stdio.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static int count = 0;


void set_CB(struct QsParameter *p,
            const void *value, size_t size, void *userData) {

    DASSERT(size == sizeof(double));

    fprintf(stderr, "                     value=%lg", *((double *) value));
}


void setCleanup_CB(struct QsParameter *p, void *userData) {

    DSPEW();
}


int bootstrap(void) {

    DSPEW("count = %d", count++);

    qsParameterSetterCreate(0, "setter", QS_DOUBLE, sizeof(double),
        set_CB, setCleanup_CB, 0, 0);

    return 0; // 0 => success
}

