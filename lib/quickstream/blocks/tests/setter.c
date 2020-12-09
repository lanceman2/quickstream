// This is used in test ../../../../tests/233_simpleGetToSetControllers
// and ../../../../tests/243_simpleGetToSetNControllers
//
// Editing this file will likely break these tests.


#include <stdio.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static int count = 0;


int set_CB(struct QsParameter *p,
            void *value, size_t size, void *userData) {

    DASSERT(size == sizeof(double));

    WARN("         SETTING value = %lg", *((double *) value));

    //if(*(double*)value > 10.0) return 1;

    return 0;
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

