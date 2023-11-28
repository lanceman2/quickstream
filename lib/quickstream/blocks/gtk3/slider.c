// This file is NOT linked with the GTK3 libraries.

#define _GNU_SOURCE
#include <math.h>
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


static struct QsParameter *getter, *active;

static bool isActive = false;


// This Slider will be added into the struct Window in the widget list.
//
static struct Slider slider = {
    .widget.type = Slider,
    // numTicks is the only variable that effects the GTK3 widget
    // internals.  We keep the GTK scale (that is the slider GTK widget)
    // with a fixed -1 to +1 scaling and use a linear transformation in
    // this file to get user values.
    //
    .numTicks = 0,
    .value = NAN
};


// parameter variables that we use to produce values for the block user.
//
// This defines simple linear scaling along with digitization.
//
// The values will be continuous if numValues == 0.
//
// min < clipMin < clipMax < max
//
// min and max are used to have nice round numbers on the two ends so the
// scale does not look like shit.  clipMin and clipMax are output and
// input value limits.  Examples:
//
//  (min, clipMin, clipMax, max, scale, numValues)
//              = (-1.0, -1.0, 1.0, 1.0, 1.0, 0) 
//  (min, clipMin, clipMax, max, scale, numValues)
//              = (-1.0, -0.9, 0.9, 1.0, 1.0, 19)
//
static double min = -1.0, clipMin = -1.0, clipMax = 1.0, max = 1.0,
              scale = 1.0;
static uint32_t numValues = 0; // continuous if numValues == 0

static char *fmt = 0;


static double GetValue(double val) {

    // val is at or between -1.0 and 1.0.
    double delta = max - min;

    double out = min + (val + 1.0)*delta/2.0;

    if(numValues) {

        DASSERT(numValues > 1);

        uint32_t n = ((numValues - 1)*((out - min)/delta)) + 0.5;

        out = min + n * delta/(numValues - 1);
    }

    if(out < clipMin)
        out = clipMin;
    else if(out > clipMax)
        out = clipMax;

    return out;
}


// Set the value that came from a block user that see the values
// as constrained by how this slider is configured.  Like for
// example values like a radio station frequency: 1.053e+8 Hz.
// Not the slider scale (-1 to +1).
static
void SetSliderValue(double val) {

    // Convert the val to slider value [-1, 1]
    //
    // 1. clip it.
    if(val < clipMin)
        val = clipMin;
    else if(val > clipMax)
        val = clipMax;
    // 2. Linearly transformation to [-1, +1]
    val = -1.0 + (val - min) * (2.0) / (max - min);

    if(slider.setSliderValue)
        slider.setSliderValue(&slider, val);
    else
        slider.value = val;
}


// This is a user telling the slider where to be.
//
// See comments in _run.c:SetSliderValue().
//
static
int Value_setter(const struct QsParameter *p, double *val,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    SetSliderValue((*val)/scale);
    return 0;
}


// This is called by a thread not from a thread pool; a GTK callback.
//
static
char *SetDisplayedValue(const struct Slider *s, double val) {

//static int count=0;
// Wow, GTK3 calls this a lot when the pointer is moving near this scale
// widget
//DSPEW("count=%d", ++count);

    // Note: this value does not include the scale factor.
    char *ret = mprintf(fmt, GetValue(val));

    return ret;
}


static
char *Attributes_config(int argc, const char * const *argv,
            void *userData) {

    if(argc < 5) return QS_CONFIG_FAIL;


    char *ret = QS_CONFIG_FAIL;

    // attributes min clipMin clipMax max [scale [numValues [numTicks [FMT]]]]

    // parameter values we need to test "t".
    //
    double tmin = min, tclipMin = clipMin,
           tclipMax = clipMax, tmax = max;
    uint32_t tnumValues = numValues, tnumTicks = slider.numTicks;

    tmin = qsParseDouble(tmin/*default value*/);
    tclipMin = qsParseDouble(tclipMin);
    tclipMax = qsParseDouble(tclipMax);
    tmax = qsParseDouble(tmax);

    if(tmin > tclipMin || tclipMin >= tclipMax || tclipMax > tmax) {
        WARN("Invalid slider attributes");
        goto finish;
    }

    min = tmin;
    clipMin = tclipMin;
    clipMax = tclipMax;
    max = tmax;

    if(argc >= 6)
        scale = qsParseDouble(scale);

    if(argc >= 7)
        qsParseUint32tArray(tnumValues, &tnumValues, 1);

    if(argc >= 8)
        qsParseUint32tArray(tnumTicks, &tnumTicks, 1);

    if(argc >= 9) {
        DZMEM(fmt, strlen(fmt));
        free(fmt);
        fmt = strdup(argv[8]);
        ASSERT(fmt, "strdup() failed");
    }


    if(tnumValues) {
        if(tnumValues == 1)
            // The slider can't have one value.
            tnumValues = 0;
    }

    if(tnumTicks) {
        if(!tnumValues || tnumTicks == 1)
            tnumTicks = 0;
        else if(tnumTicks > tnumValues)
            tnumTicks = tnumValues;
    }

    numValues = tnumValues;

    if(tnumTicks != slider.numTicks) {
        slider.numTicks = tnumTicks;
        if(slider.setSliderRange)
            slider.setSliderRange(&slider);
    }

    double val = INFINITY;

    if(argc >= 10) {
        // We did not use the qsParse thingy last arg so we need to
        // advance the parser with this:
        qsParseAdvance(1);

        val = qsParseDouble(val);
        if(isnormal(val))
            SetSliderValue(val);
    }

    DSPEW("attributes min=%lg clipMin=%lg clipMax=%lg "
            "max=%lg scale=%lg numValues=%" PRIu32
            " numTicks=%" PRIu32
            " fmt=\"%s\" value=%lg",
            min, clipMin, clipMax, max, scale,
            tnumValues, tnumTicks, fmt, val);

    if(isnormal(val)) 
        ret = mprintf("attributes %lg %lg %lg %lg %lg %"
                PRIu32 " %" PRIu32 " \"%s\" %lg",
                min, clipMin, clipMax, max, scale,
                numValues, tnumTicks, fmt, val);
    else
        ret = mprintf("attributes %lg %lg %lg %lg %lg %"
                PRIu32 " %" PRIu32 " \"%s\"",
                min, clipMin, clipMax, max, scale,
                numValues, tnumTicks, fmt);

finish:

    return ret;
}


static double lastValueOut = NAN;

// Feedback from GtkWidget, _run.c.
static void
SetValue(double *val) {

    double x = scale * GetValue(*val);

    // We don't getter push if the value did not change.
    if(lastValueOut != x) {
        lastValueOut = x;
        qsGetterPush(getter, &x);
    }
}


// Feedback from GtkWidget, _run.c.
static void
SetActive(bool *isActive) {

//DSPEW("Block: %s    isActive=%d", qsBlockGetName(), *isActive);

    qsGetterPush(active, isActive);
}


int declare(void) {

    slider.setDisplayedValue = SetDisplayedValue;

    // We need fmt set before CreateWidget() in case GTK is going to
    // immediately allocate the GTK slider widget in CreateWidget();
    // which will call slider.c:SetDisplayedValue() and access fmt.
    fmt = strdup("%5.5lf");
    ASSERT(fmt, "strdup() failed");

    qsCreateSetter("value",
        sizeof(double), QsValueType_double, 0/*0 == no initial value*/,
        (int (*)(const struct QsParameter *, const void *,
            uint32_t readCount, uint32_t queueCount,
            void *)) Value_setter);

    getter = qsCreateGetter("value", sizeof(double),
            QsValueType_double, 0/*0 == no initial value*/);

    active = qsCreateGetter("active", sizeof(isActive),
            QsValueType_bool, &isActive);


    char *text = mprintf("attributes %lg %lg %lg %lg %lg %"
            PRIu32 " %" PRIu32 " \"%s\"",
            min, clipMin, clipMax, max, scale,
            numValues, slider.numTicks, fmt);

    // All slider "functional" (dynamically parameters) attribute in one
    // configuration thingy:
    qsAddConfig((char *(*)(int argc, const char * const *argv,
            void *userData)) Attributes_config, "attributes",
            /* TODO: We really need to take long strings like this out of
             * the C source files. */
            "set slider attributes.  "
            "min will be the minimum end of the slider.  "
            "clipMin will be the minimum value that comes out of "
            "the slider.  "
            "clipMax will be the maximum value that comes out of "
            "the slider.  "
            "max will be the maximum end of the slider.  "
            "Note: min <= clipMin < clipMax <= max.  "
            "scale will multiple the value coming out of the slider.  "
            "numValues will limit the number of values coming out.  "
            "numTicks will add sticky tick marks to the slider.  "
            "FMT is a printf format string for one double, like "
            "for example \"%lg\""/*desc*/,
            "attributes min clipMin clipMax max "
            "[scale [numValues [numTicks [FMT]]]]",
            text);

    DZMEM(text, strlen(text));
    free(text);

    // So the GTK3 widget can tell this block of a value change.
    slider.setValue = qsAddInterBlockJob(
            (void (*)(void *)) SetValue,
            sizeof(double), 10/*queueMax*/);

    // So the GTK3 widget can tell this block that the slider is currently
    // moving (active) or not.
    slider.setActive = qsAddInterBlockJob(
            (void (*)(void *)) SetActive,
            sizeof(bool), 10/*queueMax*/);

    struct Window *win = CreateWidget(&slider.widget);
    DASSERT(win);

    DSPEW();
    return 0;
}


int undeclare(void *userData) {

    DestroyWidget(&slider.widget);

    DASSERT(fmt);
    DZMEM(fmt, strlen(fmt));
    free(fmt);
    fmt = 0;

    return 0;
}
