// Testing qsQtApp.cpp (read comments there-in) and libqsQtApp.so.

#include <inttypes.h>
//#include <string.h>
//#include <stdlib.h>
//#include <pthread.h>

#include <QtWidgets/QtWidgets>
#include "qsQtApp.h"

#include "debug.h"


static uint32_t count = 0;


static
void RunWindow(void) {

    ERROR("                window count=%" PRIu32, ++count);
    QWidget window;
    window.resize(320, 440);
    window.show();
    qsQtApp_exec();
    ERROR();
}


static
void RunApp(void) {

    qsQtApp_construct();
    RunWindow();
    RunWindow();
    qsQtApp_destroy();

    qsQtApp_construct();
    RunWindow();
    RunWindow();
    qsQtApp_destroy();
}


int main(void) {

    // See if QApplication is robust under reuse.
    RunApp();
    RunApp();
    // Who knows, QApplication may not even leak memory.
    // I don't think I'll hold my breath for that one.
    return 0;
}
