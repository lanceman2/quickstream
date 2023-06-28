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


static struct QsParameter *getter;

// This mutex will be shared with all the widgets from all the
// widget blocks for a given graph.  We just have one top level
// window per graph.
static pthread_mutex_t *mutex = 0;


// This Button will be added into the struct Window in the widget list.
//
static struct Button button = {
    .widget.type = Button
};


// This is the Setter parameter callback that can
// toggle the button.
static
int Value_setter(const struct QsParameter *p, bool *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    CHECK(pthread_mutex_lock(mutex));

    DSPEW("*value=%d", *value);

    if(button.value == *value)
        goto finish;

    button.value = *value;

    if(!button.setButtonValue)
        goto finish;

    button.setButtonValue(&button, button.value);

finish:

    CHECK(pthread_mutex_unlock(mutex));

    return 0;
}


// Feedback from GtkWidget.
//
// The widget button was clicked.
// This is called asynchronously by the GTK widget callback, and hence
// runs in as a job in with thread pool worker thread.
static void
SetValue(const bool *value) {

DSPEW("*value=%d", *value);
    CHECK(pthread_mutex_lock(mutex));
    // Locked so we can access button.value.

    button.value = *value;

    CHECK(pthread_mutex_unlock(mutex));

    // No mutex lock needed for this call, value it in this function call
    // stack.
    qsGetterPush(getter, value);
}


int declare(void) {

    struct Window *win = CreateWidget(&button.widget);
    DASSERT(win);
    mutex = win->mutex;
    DASSERT(mutex);

    qsCreateSetter("value",
        sizeof(button.value), QsValueType_bool, 0/*0=no initial value*/,
        (int (*)(const struct QsParameter *, const void *,
            uint32_t readCount, uint32_t queueCount,
            void *)) Value_setter);

    getter = qsCreateGetter("value", sizeof(button.value),
            QsValueType_bool, 0/*0=no initial value*/);


    // So the GTK3 widget can tell this block of a value change.
    button.setValue = qsAddInterBlockJob(
            (void (*)(void *)) SetValue,
            sizeof(bool), 7/*queueMax*/);

    DSPEW();
    return 0;
}


int undeclare(void *userData) {

   DestroyWidget(&button.widget);

   return 0;
}
