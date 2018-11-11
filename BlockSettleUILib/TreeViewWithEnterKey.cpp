

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

   auto selModel = selectionModel();
   if (selModel != nullptr) {
      selModel->select(currentIndex(),
         QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
   }
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
   // Disabled from the BST-1401 request
#if 0
   if (!indexAt(event->pos()).isValid()) {
      auto selModel = selectionModel();
      if (selModel != nullptr) {
         selModel->clear();
      }
   }
#endif

   QTreeView::mouseReleaseEvent(event);
}
