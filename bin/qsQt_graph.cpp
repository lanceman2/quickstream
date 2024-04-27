
#include <QtCore/QObject>
#include <QtWidgets/QtWidgets>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "qsQt_graph.h"
#include "qsQt_block.h"


class Graph : public QGraphicsView
{
    Q_OBJECT // MOC voodoo

public:
    explicit Graph(const char *blockPath);
    ~Graph(void);

    struct QsGraph *qsGraph;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void scaleView(qreal scaleFactor);
};


void Graph::scaleView(qreal scaleFactor) {

    qreal factor = transform().scale(scaleFactor, scaleFactor).
        mapRect(QRectF(0, 0, 1, 1)).width();
    if(factor < 0.07 || factor > 100)
        return;

    scale(scaleFactor, scaleFactor);
}


void Graph::keyPressEvent(QKeyEvent *event) {

    switch (event->key()) {
    case Qt::Key_Up:
        break;
    case Qt::Key_Down:
        break;
    case Qt::Key_Left:
        break;
    case Qt::Key_Right:
        break;
    case Qt::Key_Plus:
        scaleView(1.2F);
        return;
    case Qt::Key_Minus:
        scaleView(1.0F/1.2F);
        return;
    case Qt::Key_Space:
    case Qt::Key_Enter:
        break;
    default:
        QGraphicsView::keyPressEvent(event);
    }
}


Graph::Graph(const char *blockPath) {

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    scene->setSceneRect(-500, -500, 1000, 1000);
    setScene(scene);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    scale(1.0F, 1.0F);
    setMinimumSize(10, 10);
}

Graph::~Graph(void) {

    ERROR();

}


QWidget *CreateGraph(const char *blockPath) {

    Graph *graph = new Graph(blockPath);
    graph->show();

    { // Graph Overlay buttons:
        const int padding = 10;
        const int y = padding;
        int x = padding;

        QAbstractButton *b = new QCheckBox("Run", graph);
        b->setCheckable(true);
        b->move(x, y);
        b->show();
        b->setToolTip("Toggle Running the Flow Graph.  "
                "Blocks can run start(), flow(), "
                "and stop() functions.");
        x += b->width() + padding;

        b = new QCheckBox("Halt", graph);
        b->setCheckable(true);
        b->move(x, y);
        b->show();
        b->setToolTip("Toggle Halting the Flow Graph.  "
                "Blocks don't know they are halted.");
        x += b->width() + padding;

        b = new QPushButton("Save as ...", graph);
        b->move(x, y);
        b->show();
        b->setToolTip("Save the Flow Graph to a Selected File");
        x += b->width() + padding;

        b = new QPushButton("Save", graph);
        b->move(x, y);
        b->setEnabled(false);
        b->show();
        b->setToolTip("Save the Flow Graph to the Same "
                "Selected File Again");
        x += b->width() + padding;

        b = new QPushButton("Hide", graph);
        b->move(x, y);
        b->show();
        b->setToolTip("Hide this Button Bar");
     }

    CreateBlock(0, graph, "foo"/*block path*/);
    CreateBlock(0, graph, "foo"/*block path*/);

    return graph;
}

