#ifndef CUSTOMCHARTVIEW_H
#define CUSTOMCHARTVIEW_H

#include <QChartView>

using namespace QtCharts;

class CustomChartView : public QChartView
{
   Q_OBJECT

public:
    CustomChartView(QWidget *parent = nullptr);
    void enableZoom(bool bEnable) { bZoomEnabled_ = bEnable; }
    void enableDrag(bool bEnable) { bDragEnabled_ = bEnable;  }

protected:
   void mousePressEvent(QMouseEvent *);
   void mouseReleaseEvent(QMouseEvent *);
   void mouseMoveEvent(QMouseEvent *);
   void wheelEvent(QWheelEvent *);

signals:
   void chartScrolled(qreal dx, qreal dy);
   void chartZoomed(QWheelEvent *);
   void chartZoomReset();

public slots:
   void onChartScrolled(qreal dx, qreal dy);
   void onChartZoomed(QWheelEvent *);
   void onChartZoomReset();

private:
   void performZoom(QWheelEvent *ev);
   QPointF lastMousePos_;
   qreal zoomFactor_ = 1.0;
   qreal minZoom_ = 1.25;
   qreal maxZoom_ = 0.25;
   bool bZoomEnabled_;
   bool bDragEnabled_;

};

#endif // CUSTOMCHARTVIEW_H