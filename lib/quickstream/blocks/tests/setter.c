// This is used in test ../../../../tests/233_simpleGetToSetControllers
// and ../../../../tests/243_simpleGetToSetNControllers
//
// Editing this file will likely break these tests.


#include <stdio.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static int count = 0;
static struct QsParameter *p = 0;


static
int set_CB(struct QsParameter *p, void *value, void *userData) {

    NOTICE("         SETTING value = %lg", *((double *) value));

    //if(*(double*)value > 10.0) return 1;

    return 0;
}


int declare(void) {

    DSPEW("count = %d", count++);

    double initVal = 0.055;

    p = qsParameterSetterCreate(0, "setter", QsDouble, sizeof(double),
        set_CB, 0, QS_SETS_WHILE_PAUSED, &initVal/*initialValue*/);

    return 0; // 0 => success
}

int start(uint32_t numIn, uint32_t numOut) {

    double value;
    qsParameterGetValue(p, &value);

    NOTICE("       value=%lg\n", value);

    return 0;
}
