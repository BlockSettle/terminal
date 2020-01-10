/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "OrdersView.h"
#include "OrderListModel.h"
#include "UiUtils.h"

#include <QHeaderView>
#include <QPainter>


//
// OrdersView
//

OrdersView::OrdersView(QWidget *parent)
   : TreeViewWithEnterKey(parent)
   , model_(nullptr)
{
   connect(this, &QTreeView::collapsed, this, &OrdersView::onCollapsed);
   connect(this, &QTreeView::expanded, this, &OrdersView::onExpanded);
}

void OrdersView::initWithModel(OrderListModel *model)
{
   model_ = model;
   connect(model, &OrderListModel::rowsInserted, this, &OrdersView::onRowsInserted);
   connect(model, &OrderListModel::selectRow, this, &OrdersView::onSelectRow, Qt::QueuedConnection);
}

void OrdersView::drawRow(QPainter *painter, const QStyleOptionViewItem &option,
   const QModelIndex &index) const
{
   QTreeView::drawRow(painter, option, index);

   if (!isExpanded(index)) {
      const auto it = hasNewItems_.find(index);

      if (it != hasNewItems_.cend() && it->second) {
         const int x = header()->sectionViewportPosition(OrderListModel::Header::Product);
         const int y = option.rect.topLeft().y();
         const int width = option.rect.height();

         const QPixmap pixmap(QLatin1String(":/ICON_DOT"));
         const QRect r(x + (width > pixmap.width() ? width - pixmap.width() : 0) / 2,
            y + (width > pixmap.height() ? width - pixmap.height() : 0) / 2,
            qMin(pixmap.width(), width), qMin(pixmap.height(), width));
         painter->drawPixmap(r, pixmap, pixmap.rect());
      }
   }
}

void OrdersView::onSelectRow(const QPersistentModelIndex &row)
{
   if (!row.isValid()) {
      return;
   }

   selectionModel()->select(QModelIndex(row),
      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

   if (row.parent().isValid()) {
      scrollTo(row.parent(), QAbstractItemView::EnsureVisible);
   }
   scrollTo(row, QAbstractItemView::EnsureVisible);
}

void OrdersView::onRowsInserted(const QModelIndex &parent, int first, int)
{
   if (!parent.isValid()) {
      return;
   }

   if (!collapsed_.contains(UiUtils::modelPath(parent, model_))) {
      expand(parent);
   }
   else {
      setHasNewItemFlag(parent, true);
   }

   if (selectionModel()->hasSelection()) {
      scrollTo(selectionModel()->selectedIndexes().at(0), QAbstractItemView::EnsureVisible);
   }
}

void OrdersView::onCollapsed(const QModelIndex &index)
{
   if (index.isValid()) {
      collapsed_.append(UiUtils::modelPath(index, model_));

      viewport()->update();
   }
}

void OrdersView::onExpanded(const QModelIndex &index)
{
   if (index.isValid()) {
      collapsed_.removeOne(UiUtils::modelPath(index, model_));

      setHasNewItemFlag(index, false);
   }
}

void OrdersView::setHasNewItemFlag(const QModelIndex &index, bool value)
{
   auto it = hasNewItems_.find(index);

   if (it != hasNewItems_.end()) {
      it->second = value;
   } else {
      hasNewItems_[index] = value;
   }

   if (index.parent().isValid()) {
      setHasNewItemFlag(index.parent(), value);
   }

   viewport()->update();
}
