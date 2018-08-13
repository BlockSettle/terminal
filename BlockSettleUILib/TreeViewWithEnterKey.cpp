

#include <QKeyEvent>
#include <QMouseEvent>

#include "TreeViewWithEnterKey.h"


//
// TreeViewWithEnterKey
//

TreeViewWithEnterKey::TreeViewWithEnterKey(QWidget *parent)
   : QTreeView(parent)
{
}

QStyleOptionViewItem TreeViewWithEnterKey::viewOptions() const
{
   return QTreeView::viewOptions();
}

void TreeViewWithEnterKey::activate()
{
   setFocus();

   selectionModel()->select(currentIndex(),
      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void TreeViewWithEnterKey::keyPressEvent(QKeyEvent *event)
{
   if (currentIndex().isValid()) {
      if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
         emit enterKeyPressed(currentIndex());

         return;
      }
   }

   QTreeView::keyPressEvent(event);
}

void TreeViewWithEnterKey::mouseReleaseEvent(QMouseEvent *event)
{
   if (!indexAt(event->pos()).isValid()) {
      selectionModel()->clear();
   }

   QTreeView::mouseReleaseEvent(event);
}
