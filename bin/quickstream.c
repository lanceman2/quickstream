#include <stdio.h>
#include "../lib/debug.h"
#include "../include/quickstream/app.h"


int main(int argc, const char * const *argv) {

    DSPEW("QUICKSTREAM_VERSION=\"%s\"", QUICKSTREAM_VERSION);

    struct QsApp *app = qsAppCreate();


    qsAppDestroy(app);

    return 0;
}
