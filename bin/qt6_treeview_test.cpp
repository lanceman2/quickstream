#include <stdlib.h>

#include <QtWidgets/QtWidgets>

#include "../include/quickstream.h"
#include "../lib/debug.h"
#include "../lib/mprintf.h"

#include "qsQt_treeview.h"



int main(int argc, char *argv[]) {

    QApplication app(argc, argv);

    char *path = mprintf("%s/quickstream/misc/test_blocks", qsLibDir);

    // This will add more blocks to the treeview.
    setenv("QS_BLOCK_PATH", path, 1);
    memset(path, 0, strlen(path));
    free(path);


    QTreeView *view = MakeTreeview();

    const auto screenSize = view->screen()->availableSize();
    view->resize({screenSize.width()/2, screenSize.height()/2});
    view->show();
    return QCoreApplication::exec();
}
