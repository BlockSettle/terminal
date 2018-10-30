#include "CustomTreeWidget.h"
#include <QMouseEvent>
#include <QDebug>

CustomTreeWidget::CustomTreeWidget(QWidget *parent) : 
   QTreeWidget(parent)
{

}

void CustomTreeWidget::mouseReleaseEvent(QMouseEvent *ev) {
   // will use left click in to open address page 
   if (ev->button() == Qt::LeftButton) {
      //QTreeWidgetItem *item = itemAt(ev->pos());
   }
   QTreeWidget::mouseReleaseEvent(ev);
}