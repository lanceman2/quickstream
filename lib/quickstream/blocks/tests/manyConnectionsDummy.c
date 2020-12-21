// Editing this file will likely break some tests.

#include <stdio.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"


static
int setCallback(struct QsParameter *p, const void *value, void *userData) {
    return 0;
}


int declare(void) {

    double val = 0.055;

    qsParameterSetterCreate(0, "setter0", QsDouble, sizeof(val),
            setCallback, 0, QS_SETS_WHILE_PAUSED, &val/*initialValue*/);
    qsParameterConstantCreate(0, "constant0", QsDouble,
            sizeof(val), setCallback, 0, &val);
    qsParameterGetterCreate(0, "getter0", QsDouble,
            sizeof(val), &val);

    qsParameterSetterCreate(0, "setter1", QsDouble, sizeof(val),
            setCallback, 0, QS_SETS_WHILE_PAUSED, &val/*initialValue*/);
    qsParameterConstantCreate(0, "constant1", QsDouble,
            sizeof(val), setCallback, 0, &val);
    qsParameterGetterCreate(0, "getter1", QsDouble,
            sizeof(val), &val);

    qsParameterSetterCreate(0, "setter2", QsDouble, sizeof(val),
            setCallback, 0, QS_SETS_WHILE_PAUSED, &val/*initialValue*/);
    qsParameterConstantCreate(0, "constant2", QsDouble,
            sizeof(val), setCallback, 0, &val);
    qsParameterGetterCreate(0, "getter2", QsDouble,
            sizeof(val), &val);


    return 0; // 0 => success
}


int flow(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs) {

    return 0;
}

int flush(void *buffer[], const size_t len[],
            uint32_t numInputs, uint32_t numOutputs) {

    return 0;
}
