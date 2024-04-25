#include <inttypes.h>

#include <QtWidgets/QtWidgets>


#include "../lib/debug.h"
#include "../include/quickstream.h"

#include "qsQt.h"
#include "qsQt_treeview.h"


class Window : public QWidget {

    public:

        Window(const char *blockPath=0);

    private:

        ~Window(void);
};


// This is the only exposed function from this file.
void CreateWindow(const char *blockPath) {
    new Window(blockPath);
    // This Window will automatically have delete called on it.
}

#define W 1400
#define H 700

// https://doc.qt.io/qt-6/qtwidgets-tutorials-widgets-toplevel-example.html
// Says:
// it is up to the developer to keep track of the top-level widgets in an
// application.
//
Window::Window(const char *blockPath_in):
    QWidget((QWidget *)0/*0 = top level window widget*/){

    size_t w = W, h = H;

    QWidget *win = this;
    // This Window object will automatically have delete called on it.
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->resize(w, h);
    win->show();

    DSPEW("blockPath=\"%s\"", blockPath_in);

    win->setWindowTitle("quickstream");
    //QApplication::translate("toplevel", "Top-level widget"));

    QVBoxLayout *layout = new QVBoxLayout(win);
    layout->setContentsMargins(
            /* adds a border around the edge of the window */
            0/*left*/, 0/*top*/, 0/*right*/, 0/*bottom*/);
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    layout->addWidget(splitter);


    AddTab("TabName1", splitter);
    AddTab("TabName2");
    AddTab("TabName3");
    AddTab("TabName4");

    MakeTreeview(splitter);

    splitter->setHandleWidth(3);
    splitter->show();
    // Reuse w, and h.
    h = w;
    if(w > 300)
        w = w - 300;
    else
        w /= 2;
    splitter->setSizes({(int) w, (int) (h - w)});
    splitter->setChildrenCollapsible(true);
}


Window::~Window(void) {

    // TODO: cleanup anything else that may need cleaning up.

DSPEW();
}

