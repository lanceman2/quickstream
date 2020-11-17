#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "parameter.h"
#include "debug.h"
#include "block.h"

#include "../include/quickstream/block.h"



struct QsParameter *
qsParameterSetterCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize,
        void (*setCallback)(struct QsParameter *p,
            const void *value, size_t size, void *userData),
        void (*cleanup)(struct QsParameter *, void *userData),
        void *userData, uint32_t flags) {

    return 0;
}


struct QsParameter *
qsParameterGetterCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize) {


    return 0;
}


struct QsParameter *
qsParameterConstantCreate(struct QsBlock *block, const char *pname,
        enum QsParameterType type, size_t psize) {


    return 0;
}

