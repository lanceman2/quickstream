// Qt has a generic tab-bar QTabBar which has no notebook page like widget
// associated with each tab.  The pre-made tab-notebook thing is managed
// with a QTabWidget object.  Me, coming from a GTK widgets background,
// the Qt tabs terminology seems a little off too me.


// To add a tab is to add a graph.

#include <QtCore/QObject>
#include <QtWidgets/QtWidgets>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "qsQt_notebook.h"
#include "qsQt_graph.h"


class Notebook : public QTabWidget
{
    Q_OBJECT // MOC voodoo

public:
    explicit Notebook(QWidget *window, QWidget *parent);
    ~Notebook(void);

private slots: // MOC voodoo.
    void closeTab(const int& index);

private:
    QWidget *window; // The main window that this notebook is in.
};


void Notebook::closeTab(const int& index) {

    if(count() == 1)
        // This is the last tab left, that will be destroyed with the main
        // window which is a super (top) parent of these tabs.
        delete window;
    else
        delete widget(index);
}


Notebook::Notebook(QWidget *win, QWidget *parent) :
    QTabWidget(parent), window(win) {

    setTabsClosable(true);

    // MOC voodoo.  This works with Qt6 v6.7.0
    connect(this, SIGNAL(tabCloseRequested(int)),
            this, SLOT(closeTab(int)));
    show();
}

Notebook::~Notebook(void) {

    ERROR();
}

// To hell with OOP (object oriented programming).

// We did not want to expose any more Qt MOC code to the other code,
// hence we just expose these two functions (symbols) to the other
// source files:

Notebook *CreateNotebook(QWidget *window, QWidget *parent) {
    DASSERT(window);
    DASSERT(parent);
    return new Notebook(window, parent);
}

void Notebook_AddTab(Notebook *notebook, const char *name/*tab label*/,
        const char *blockPath) {

    DASSERT(notebook);
    DASSERT(name);
    DASSERT(name[0]);

    notebook->addTab(CreateGraph(blockPath), Notebook::tr(name));
}
