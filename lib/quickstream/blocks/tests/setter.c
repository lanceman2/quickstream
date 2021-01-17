// This is used in test ../../../../tests/233_simpleGetToSetControllers
// and ../../../../tests/243_simpleGetToSetNControllers
// ../../../../tests/255_constantToSetterControllers
//
// Editing this file will likely break these tests.

#include <stdio.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static struct QsParameter *p = 0;


static
int set_CB(struct QsParameter *p, const void *value, void *userData) {

    NOTICE("         SetCallback with value = %lg", *((double *) value));

    printf("%s %lg\n", qsBlockGetName(0), *((double *) value));

    if(*(double*)value > 10.0) return 1;

    return 0;
}


int declare(void) {

    double initVal = 0.055;

    p = qsParameterSetterCreate(0, "setter", QsDouble, sizeof(double),
        set_CB, 0, &initVal/*initialValue*/);

    return 0; // 0 => success
}

int start(uint32_t numIn, uint32_t numOut) {

    double value;
    qsParameterGetValue(p, &value);

    NOTICE("       value=%lg", value);

    return 0;
}
