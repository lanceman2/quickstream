// This file is NOT linked with the wsServer library.

#define _GNU_SOURCE
#include <link.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>

#include "wsServer.h"

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../mprintf.h"


static struct QsParameter *launch_getter;

static struct wsServer server;

// local launch state
static bool launch = false; // initial launch value


// Setter to turn the server on/off.
static
int Launch_setter(const struct QsParameter *p, bool *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

DSPEW("*value=%d", *value);

    return 0;
}


int declare(void) {

    qsCreateSetter("launch",
        sizeof(launch), QsValueType_bool, &launch/*initial value*/,
        (int (*)(const struct QsParameter *, const void *,
            uint32_t readCount, uint32_t queueCount,
            void *)) Launch_setter);

    launch_getter = qsCreateGetter("launch", sizeof(launch),
            QsValueType_bool, 0/*0=no initial value*/);

// Remove compiler warning with &server being used here.
ERROR("This block does nothing yet, we"
" have not finished writing it yet. server=%p", &server);


    // MORE CODE HERE ....

    return 0;
}


int undeclare(void *userData) {

    // MORE CODE HERE ....

   return 0;
}
