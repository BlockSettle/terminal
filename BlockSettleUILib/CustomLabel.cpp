#include "CustomLabel.h"
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QRegularExpression>
#include <QTimer>

const QRegularExpression kRxSuffix(QStringLiteral(R"(\s\[.*\]$)"), QRegularExpression::CaseInsensitiveOption);

CustomLabel::CustomLabel(QWidget *parent) :
    QLabel(parent)
{
   if (property("showToolTipQuickly").toBool()) {
      setMouseTracking(true);
   }
}

void CustomLabel::mouseReleaseEvent(QMouseEvent *ev) {
   // on right click copy label text into clipboard and show a tooltip letting user know about it
   if (ev->button() == Qt::RightButton) {
      if (property("copyToClipboard").toBool()) {
         QClipboard *clipboard = QApplication::clipboard();
         // Clean
         QString value = text();
         if (property("trimSuffix").toBool()) {
            value.remove(kRxSuffix);
         }
         clipboard->setText(value);
         // placing the tooltip in a timer because mouseReleaseEvent messes with it otherwise
         QTimer::singleShot(50, [=] {
            QToolTip::showText(this->mapToGlobal(QPoint(0, 3)), tr("Copied '%1' to clipboard.").arg(value), this);
         });
      }
   }
   QLabel::mouseReleaseEvent(ev);
}

void CustomLabel::mouseMoveEvent(QMouseEvent *ev) {
   if (!toolTip_.isEmpty()) {
      QToolTip::showText(mapToGlobal(QPoint(0, 7)), toolTip_, this);
   }
   QLabel::mouseMoveEvent(ev);
}
