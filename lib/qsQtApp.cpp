// This is an attempt to make it look like we can make many QApplication
// objects, even though we can only make one at a time.  We let all the
// codes that call qsQtApp_init() share that one QApplication object.
// Since there can be only one QApplication object at a time per process,
// we do not need to return a value from qsQtApp_init().  If it fails it
// asserts.  I figure your app will be screwed anyway if the QApplication
// constructor fails or memory allocation fails.

// This is like the gtk_init() problem addressed in the file ./qsGtk_init.c
// but easier to solve given Qt6 is a little more robust than GTK3.  And
// that's by their design; I do not run/control the world.

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <QtWidgets/QtWidgets>

#include "debug.h"

// TODO: Should this be called "qsQt6" ?  Maybe not until we start using
// Qt version 7 too.  But, using Qt 7 under the name Qt7 will not conflict
// with this name, so fuck it, good enough for now.
#include "qsQtApp.h"


// We need this code to be thread safe.  Performance is not thought to be
// an issue yet.  A little memory usage, but oh well.
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// We keep a count of the number of QApplication objects that have been
// requested to be made, but we only make one at most.  QApplication
// objects are not robust that way; they let you make more than one, but
// tests show that the program hangs in the second QApplication
// constructor call, if there exists one QApplication object already.
static uint32_t appCount = 0;

static int argc = 1;
static char **argv;
static QApplication *qa = 0;


// From https://doc.qt.io/qt-6/qapplication.html#QApplication
// QApplication::QApplication(int &argc, char **argv) The data referred to
// by argc and argv must stay valid for the entire lifetime of the
// QApplication object.  Good to know.

void qsQtApp_construct(void) {

    CHECK(pthread_mutex_lock(&mutex));

    if(appCount == 0) {

        DASSERT(qa == 0);

        // The QApplication::QApplication() can mess with argc and argv;
        // I'd call that bad form; but we don't want to write our own
        // widget toolkit just yet.
        //
        // Allocate argv.  C++ needs a cast to set argv.
        //
        argv = (char **) calloc(1, (argc+1)*sizeof(argv));
        ASSERT(argv, "calloc(1,%zu) failed", (argc+1)*sizeof(argv));
        *argv = strdup("quickstream");
        ASSERT(*argv, "strdup() failed");

        qa = new QApplication(argc, argv);
        ASSERT(qa);
    }

    ++appCount;

    CHECK(pthread_mutex_unlock(&mutex));
}


void qsQtApp_exec(void) {

    ASSERT(qa);

    // Oh what fun we'll have, making the Qt main loop run with non-Qt
    // pthreads.  I'm sure the Qt developers will say it can't be done,
    // but I know better.
    if(qa->exec())
        // TODO: Do we need to address some failure modes?
        ERROR("QApplication::exec() failed");
}


// For every qsQtApp_construct() call there must be a qsQtApp_destroy()
// some time later.
void qsQtApp_destroy(void) {

    CHECK(pthread_mutex_lock(&mutex));

    --appCount;

    if(!appCount) {
        DASSERT(argv);
        DASSERT(*argv);
        // Did QApplication::QApplication() mess with argc?
        // I'd hope not.
        ASSERT(argc == 1);

        DASSERT(qa);

#ifdef DEBUG
        memset(*argv, 0, strlen(*argv));
#endif
        free(*argv);
#ifdef DEBUG
        memset(argv, 0, (argc+1)*sizeof(argv));
#endif
        free(*argv);

        delete qa;
        qa = 0;
    }

    CHECK(pthread_mutex_unlock(&mutex));
}

