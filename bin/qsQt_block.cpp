#include <QtCore/QObject>
#include <QtWidgets/QtWidgets>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "qsQt_block.h"



class Block : public QGraphicsItem
{
public:
    Block(struct QsGraph *qsGraph, struct QsBlock *qsBlock,
            QGraphicsView *graphicsView);
    ~Block(void);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter,
            const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    enum { Type = UserType + 1 };
    int type() const override { return Type; }


protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QGraphicsView *graphicsView;

    struct QsGraph *qsGraph;
    struct QsBlock *qsBlock;
};


Block::~Block(void) {

    ERROR();
}


Block::Block(struct QsGraph *qsGraph, struct QsBlock *qsBlock,
        QGraphicsView *graphicsView)
    : graphicsView(graphicsView), qsGraph(qsGraph) {

    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(-1);
    show();
}


QRectF Block::boundingRect() const
{
    return QRectF(-100, -100, 200, 200);
}

QPainterPath Block::shape() const
{
    QPainterPath path;
    //path.addEllipse(-10, -10, 20, 20);
    // Testing that this shape things does what I think it does by
    // increasing the size of this shape.  It's like we draw it three
    // times, once for boundingRect (as a pre detection, or is it a
    // wayland thing), once for detection, and once for drawing what we
    // see.
    path.addRect(-100 - 2, -100 - 2, 202, 202);
    return path;
}

void Block::paint(QPainter *painter,
        const QStyleOptionGraphicsItem *option, QWidget *) {

    if(option->state & QStyle::State_Sunken)
        painter->setPen(Qt::blue);
    else
        painter->setPen(Qt::red);

    painter->setBrush(QColor(255,255,0, 120));
    painter->drawRect(-100, -100, 200, 200);

    QRectF textRect(-100, -100, 200, 200);
    QString message(QWidget::tr("Text that can be zoomed."));

    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(14);
    painter->setFont(font);
    painter->setPen(Qt::black);
    painter->drawText(textRect, message);
}

QVariant Block::itemChange(GraphicsItemChange change, const QVariant &value) {

    switch (change) {
        case ItemPositionHasChanged:
        break;
    default:
        break;
    };

    return QGraphicsItem::itemChange(change, value);
}

void Block::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    update();
    QGraphicsItem::mousePressEvent(event);
}

void Block::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    update();
    QGraphicsItem::mouseReleaseEvent(event);
}


void CreateBlock(struct QsGraph *qsGraph,
        QGraphicsView *graphicsView, const char *blockPath) {

    DASSERT(graphicsView);
    DASSERT(blockPath);

    QGraphicsItem *item = new Block(qsGraph, 0/*qsBlock*/, graphicsView);
    QGraphicsScene *scene = graphicsView->scene();
    DASSERT(scene);
    scene->addItem(item);
    item->setPos(100, 100);
}
