// This file is NOT linked with the GTK3 libraries.

#define _GNU_SOURCE
#include <link.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../mprintf.h"

#include "all_common.h"
#include "block_common.h"


// This Text will be added into the struct Window in the widget list.
//
static struct Text text = {
    .widget.type = Text,
    .value = ""
};


static
int Value_setter(const struct QsParameter *p, char value[32],
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    //DSPEW("*value=\"%s\"", value);

    if(strcmp(value, text.value) == 0)
        goto finish;

    strncpy(text.value, value, 31);
    text.value[31] = '\0';

    if(!text.setTextValue)
        goto finish;

    text.setTextValue(&text, value);

finish:

    return 0;
}



int declare(void) {

    qsCreateSetter("value",
        32, QsValueType_string32, 0/*0=no initial value*/,
        (int (*)(const struct QsParameter *, const void *,
            uint32_t readCount, uint32_t queueCount,
            void *)) Value_setter);

    struct Window *win = CreateWidget(&text.widget);
    DASSERT(win);

    DSPEW();
    return 0;
}


int undeclare(void *userData) {

   DestroyWidget(&text.widget);

   return 0;
}
