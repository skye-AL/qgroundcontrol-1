#ifndef HEIGHTPOINT_H
#define HEIGHTPOINT_H

#include <QGraphicsItem>
#include "Waypoint.h"
#include "opmapcontrol.h"

class HeightProfile;

class HeightPoint : public QGraphicsObject
{
    //Q_OBJECT //if enabled symbol(s) not found for architecture!!!!!
public:
    HeightPoint(HeightProfile* parent, Waypoint* wp, QColor color, int listindex);


    QRectF boundingRect() const;
    QPainterPath shape() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setNumber(int const & value);
    //void updateWayoint();

public slots:
    void updateHeightPoint(Waypoint* _wp);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

    HeightProfile* parent; ///< Parent widget
    Waypoint* wp; ///< Waypoint data container this item represents.
    QColor color;
    int number;

private:


signals:
    //void move(double);



};

#endif // HEIGHTPOINT_H
