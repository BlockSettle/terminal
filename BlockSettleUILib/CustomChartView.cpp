#include "CustomChartView.h"
#include <QApplication>
#include <QCandlestickSet>
#include <QDebug>

CustomChartView::CustomChartView(QWidget *parent)
   : QChartView(parent)
   , bZoomEnabled_(false)
   , bDragEnabled_(false) {
   setDragMode(QGraphicsView::NoDrag);
   this->setMouseTracking(true);
}

void CustomChartView::setZoomFactor(const qreal &zoomFactor) {
   zoomFactor_ = zoomFactor;
   applyZoom(chart()->plotArea().center());
}

void CustomChartView::mousePressEvent(QMouseEvent *ev) {
   if (ev->button() == Qt::LeftButton) {
      if (bDragEnabled_) {
         QApplication::setOverrideCursor(QCursor(Qt::SizeHorCursor));
         lastMousePos_ = ev->pos();
         ev->accept();
      }
      QGraphicsItem *item = itemAt(ev->pos());
      if (item) {
         //qDebug() << "You clicked on item" << item;
         QCandlestickSet *candleItem = nullptr;// qobject_cast<QCandlestickSet *>(item);
         if (candleItem) {
            //qDebug() << candleItem->open() << candleItem->close();
         }
      }
      else {
         //qDebug() << "You didn't click on an item.";
      }
   }
   QChartView::mousePressEvent(ev);
}

void	CustomChartView::mouseReleaseEvent(QMouseEvent *ev) {
   if (ev->button() == Qt::LeftButton && bDragEnabled_) {
      QApplication::restoreOverrideCursor();
      qDebug() << "mouse released";
   }
   QChartView::mouseReleaseEvent(ev);
}

void CustomChartView::mouseMoveEvent(QMouseEvent *ev) {
   // pan the chart with a middle mouse drag
   if (ev->buttons() & Qt::LeftButton && bDragEnabled_) {
      auto dPos = ev->pos() - lastMousePos_;
      chart()->scroll(-dPos.x(), 0);
      emit chartScrolled(-dPos.x(), 0);
      lastMousePos_ = ev->pos();
      ev->accept();
   }

   QChartView::mouseMoveEvent(ev);
}

void CustomChartView::wheelEvent(QWheelEvent *ev) {
   if (bZoomEnabled_)
      performZoom(ev);
   qDebug() << "Zoom enabled:" << bZoomEnabled_ << "Zoom factor:" << zoomFactor_;
   QChartView::wheelEvent(ev);
}

void CustomChartView::performZoom(QWheelEvent *ev) {
   auto delta = ev->angleDelta().y();
   // don't perform zoom if the zoom is already at 
   // min or max value
   if ((zoomFactor_ == minZoom_ && delta < 0) || (zoomFactor_ == maxZoom_ && delta > 0))
      return;
   // set the zoom factor and keep it within reasonable range
   zoomFactor_ *= delta > 0 ? 0.83333333 : 1.2;
   if (zoomFactor_ < maxZoom_)
      zoomFactor_ = maxZoom_;
   if (zoomFactor_ > minZoom_)
      zoomFactor_ = minZoom_;

   /*QRectF rect = chart()->plotArea();
   QPointF c = chart()->plotArea().center();
   // reset the zoom rect
   chart()->zoomReset();
   emit chartZoomReset();
   // use the mouse x position so that the zoom
   // is focused on where the mouse cursor is
   auto cursorX = ev->posF().x();
   c.setX(cursorX);
   //qDebug() << "mouseX" << cursorX << rect;
   rect.setWidth(zoomFactor_ * rect.width());
   rect.moveCenter(c);

   // and zoom it based on the zoomFactor
   chart()->zoomIn(rect);
   emit chartZoomed(ev);

   // try to reposition the chart after zooming
   auto deltaX = chart()->plotArea().center().x() - cursorX;
   chart()->scroll(deltaX, 0);
   qDebug() << deltaX << rect;*/
   applyZoom(ev->posF());
   emit chartZoomed(ev);
}

void CustomChartView::applyZoom(const QPointF &cursor)
{
   QRectF rect = chart()->plotArea();
   QPointF c = chart()->plotArea().center();
   // reset the zoom rect
   chart()->zoomReset();
   emit chartZoomReset();
   // use the mouse x position so that the zoom
   // is focused on where the mouse cursor is
   auto cursorX = cursor.x();
   c.setX(cursorX);
   //qDebug() << "mouseX" << cursorX << rect;
   rect.setWidth(zoomFactor_ * rect.width());
   rect.moveCenter(c);

   // and zoom it based on the zoomFactor
   chart()->zoomIn(rect);
//   emit chartZoomed(ev);

   // try to reposition the chart after zooming
   auto deltaX = chart()->plotArea().center().x() - cursorX;
   chart()->scroll(deltaX, 0);
   qDebug() << deltaX << rect;
}

void CustomChartView::onChartScrolled(qreal dx, qreal dy) {
   chart()->scroll(dx, dy);
}

void CustomChartView::onChartZoomed(QWheelEvent *ev) {
   performZoom(ev);
}

void CustomChartView::onChartZoomReset() {
   chart()->zoomReset();
}
