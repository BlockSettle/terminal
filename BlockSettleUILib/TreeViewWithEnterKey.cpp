/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/


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

void TreeViewWithEnterKey::setEnableDeselection(bool enableDeselection)
{
   enableDeselection_ = enableDeselection;
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
   if (enableDeselection_) {
      if (!indexAt(event->pos()).isValid()) {
         auto selModel = selectionModel();
         if (selModel != nullptr) {
            selModel->clear();
         }
      }

   }

   QTreeView::mouseReleaseEvent(event);
}
