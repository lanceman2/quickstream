
#include <stdlib.h>
#include <string.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "app.h" // private interfaces


struct QsApp *qsAppCreate(void) {

    struct QsApp *app = calloc(1, sizeof(*app));
    ASSERT(app, "calloc(1,%zu) failed", sizeof(*app));
    app->blocks = qsDictionaryCreate();

    return app;
}


void qsAppDestroy(struct QsApp *app) {

    DASSERT(app);
    DASSERT(app->blocks);

    qsDictionaryDestroy(app->blocks);

#ifdef DEBUG
    memset(app, 0, sizeof(*app));
#endif


    free(app);
}
