#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../include/quickstream/builder.h"

#include "block.h"
#include "app.h"
#include "Dictionary.h"
#include "debug.h"



struct QsBlock *qsAppBlockLoad(struct QsApp *app, const char *fileName,
        const char *blockName) {

     
    // TODO: dlopen
    // see if isSuperBlock is defined.
    // Then allocate the correct struct.
    
    struct QsSimpleBlock *b = calloc(1, sizeof(*b));
    ASSERT(b, "calloc(1, %zu) failed", sizeof(*b));


    return (struct QsBlock *) b;
}


static int qsSimpleBlockUnload(struct QsSimpleBlock *b) {

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    return 0;
}


static int qsSuperBlockUnload(struct QsSuperBlock *b) {

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    return 0;
}




int qsBlockUnload(struct QsBlock *b) {

    DASSERT(b);

    if(b->isSuperBlock)
        qsSuperBlockUnload((struct QsSuperBlock *) b);
    else
        qsSimpleBlockUnload((struct QsSimpleBlock *) b);

    free(b);
    return 0; // success
}

