#include "CustomTreeWidget.h"
#include <QMouseEvent>
#include <QHeaderView>
#include <QClipboard>
#include <QToolTip>

CustomTreeWidget::CustomTreeWidget(QWidget *parent)
   : QTreeWidget(parent)
   , cursorHand_(false)
   , copyToClipboardColumn_(-1)
{
   setMouseTracking(true);
   header()->setMouseTracking(true);

   connect(this, &QTreeWidget::itemEntered,
      this, &CustomTreeWidget::onItemEntered);
   connect(this->header(), &QHeaderView::entered,
      this, &CustomTreeWidget::onHeaderEntered);
}

void CustomTreeWidget::mouseReleaseEvent(QMouseEvent *ev) {
   // will use left click in to open address page
   if (ev->button() == Qt::LeftButton) {
      //QTreeWidgetItem *item = itemAt(ev->pos());
   }
   else if (ev->button() == Qt::RightButton && copyToClipboardColumn_ != -1) {
      QTreeWidgetItem *item = itemAt(ev->pos());
      if (item) {
         QClipboard *clipboard = QApplication::clipboard();
         clipboard->setText(item->text(copyToClipboardColumn_));
         QPoint p = ev->pos();
         p.setY(p.y() + 3);
         // placing the tooltip in a timer because mouseReleaseEvent messes with it otherwise
         QTimer::singleShot(50, [=] {
            QToolTip::showText(this->mapToGlobal(p), tr("Copied '") + item->text(copyToClipboardColumn_) + tr("' to clipboard."), this);
         });
      }
   }
   QTreeWidget::mouseReleaseEvent(ev);
}

void CustomTreeWidget::leaveEvent(QEvent *ev) {
   // reset the mouse cursor when it leaves the tree
   resetCursor();
   QTreeWidget::leaveEvent(ev);
}

void CustomTreeWidget::mouseMoveEvent(QMouseEvent *ev) {
   if (!itemAt(ev->pos())) {
      // reset the mouse cursor when it's not hovering over an item
      resetCursor();
   }
   QTreeWidget::mouseMoveEvent(ev);
}

void CustomTreeWidget::onItemEntered(QTreeWidgetItem * item, int column) {
   if (handCursorColumns_.contains(column)) {
      // set to hand cursor when it hovers over the specified item columns
      setHandCursor();
   }
   else {
      // otherwise reset the mouse cursor
      resetCursor();
   }

   if (copyToClipboardColumns_.contains(column)) {
      copyToClipboardColumn_ = column;
   }
   else {
      copyToClipboardColumn_ = -1;
   }
}

// The mouse cursor currently doesn't reset when moving from
// TreeWidget into the header. This slot should handle it but it
// doesn't. It might be necessary to override QHeaderView and handle
// it there.
void CustomTreeWidget::onHeaderEntered(const QModelIndex &index) {
   resetCursor();
}

// This function resizes each column based on its current data width,
// it adds 5px margin but keeps column width to 250px maximum.
void CustomTreeWidget::resizeColumns() {
   // adjust the column widths based on existing data
   for (int i = 0; i < columnCount(); ++i) {
      resizeColumnToContents(i);
      // add 5px margin except for last column
      if (i < columnCount() - 1) {
         setColumnWidth(i, columnWidth(i) + 5);
      }
      // if a column is larger than 300 then force it to 300px
      //if (columnWidth(i) > 250)
      //   setColumnWidth(i, 250);
   }
}
