/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CUSTOMTREEWIDGET_H
#define CUSTOMTREEWIDGET_H

#include <QTreeWidget>
#include <QApplication>
#include <QTimer>

class CustomTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    CustomTreeWidget(QWidget *parent = nullptr);
    QList<int> handCursorColumns_;
    QList<int> copyToClipboardColumns_;
    void resizeColumns();

signals:

protected slots:
   void onItemEntered(QTreeWidgetItem *, int);
   void onHeaderEntered(const QModelIndex &);

protected:
   void mouseReleaseEvent(QMouseEvent *ev);
   void leaveEvent(QEvent *ev);
   void mouseMoveEvent(QMouseEvent *ev);
   bool cursorHand_;

   void resetCursor() {
      if (cursorHand_) {
         QApplication::restoreOverrideCursor();
         cursorHand_ = false;
      }
   }
   void setHandCursor() {
      if (!cursorHand_) {
         QApplication::setOverrideCursor(QCursor(Qt::PointingHandCursor));
         cursorHand_ = true;
      }
   }

private:
   int copyToClipboardColumn_;
};

#endif // CUSTOMTREEWIDGET_H
