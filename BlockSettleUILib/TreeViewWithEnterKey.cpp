

#include <QKeyEvent>

#include "TreeViewWithEnterKey.h"


//
// TreeViewWithEnterKey
//

TreeViewWithEnterKey::TreeViewWithEnterKey(QWidget *parent)
   : QTreeView(parent)
{
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
