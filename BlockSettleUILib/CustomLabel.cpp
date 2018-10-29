#include "CustomLabel.h"
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>

CustomLabel::CustomLabel(QWidget *parent) :
    QLabel(parent)
{

}

void CustomLabel::mouseReleaseEvent(QMouseEvent *ev)
{
   // will use left click in to open address page 
   if (ev->button() == Qt::LeftButton) {

   }
   // on right click copy label text into clipboard and show a tooltip letting user know about it
   else if (ev->button() == Qt::RightButton) {
      QClipboard *clipboard = QApplication::clipboard();
      clipboard->setText(text());
      QToolTip::showText(this->mapToGlobal(QPoint(0, 3)), tr("Copied '") + text() + tr("' to clipboard."), this);
   }
   // not calling parent's function because otherwise tooltip will not show up, for a label it's not a big deal
   //QLabel::mouseReleaseEvent(ev);
}
