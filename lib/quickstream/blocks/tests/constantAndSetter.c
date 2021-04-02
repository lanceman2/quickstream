// This is used in test ../../../../tests/257_constantToSetterInSameBlock
//
// Editing this file will likely break these tests.

#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "../../../../include/quickstream/block.h"

#include "../../../debug.h"



static
int setCallback(struct QsParameter *p, const double *value, void *userData) {

    DSPEW("parameter %s value=%lg", qsParameterGetName(p), *value);
    return 0;
}


int declare(void) {

    double val = 0.034;

    qsParameterConstantCreate(0, "constant0", QsDouble,
            sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback,
            0, &val);
    qsParameterConstantCreate(0, "constant1", QsDouble,
            sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback,
            0, &val);
    qsParameterConstantCreate(0, "constant2", QsDouble,
            sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback,
            0, &val);


    qsParameterSetterCreate(0, "setter0", QsDouble, sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback, 0, &val/*initialValue*/);
    qsParameterSetterCreate(0, "setter1", QsDouble, sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback, 0, &val/*initialValue*/);
    qsParameterSetterCreate(0, "setter2", QsDouble, sizeof(double),
            (int (*)(struct QsParameter *p, const void *v, void *u))
            setCallback, 0, &val/*initialValue*/);


    return 0; // 0 => success
}


int start(uint32_t numInPorts, uint32_t numOutPorts) {


    return 0; // success
}
