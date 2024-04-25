
#include <QtCore/QObject>
#include <QtWidgets/QtWidgets>

#include "../lib/debug.h"

class Graph : public QGraphicsView
{
    Q_OBJECT // MOC voodoo

public:
    explicit Graph(const char *blockPath);
    ~Graph(void);

    struct QsGraph *qsGraph;
};


Graph::Graph(const char *blockPath) {

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(scene);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    scale(qreal(1.0), qreal(1.0));
    setMinimumSize(400, 400);

    ERROR();
}

Graph::~Graph(void) {

    ERROR();

}


QWidget *CreateGraph(const char *blockPath) {

    return new Graph(blockPath);
}



