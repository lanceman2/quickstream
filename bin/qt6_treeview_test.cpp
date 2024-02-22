#include <stdlib.h>

#include <QtWidgets/QtWidgets>

#include "qsQt_treeview.h"

#include "../lib/debug.h"



int main(int argc, char *argv[]) {

    QApplication app(argc, argv);



    QTreeView *view = MakeTreeview();

    const auto screenSize = view->screen()->availableSize();
    view->resize({screenSize.width()/2, screenSize.height()/2});
    view->show();
    return QCoreApplication::exec();
}
