
#include "OrdersView.h"
#include "OrderListModel.h"
#include "UiUtils.h"


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
}

void OrdersView::onRowsInserted(const QModelIndex &parent, int first, int)
{
   if (!collapsed_.contains(UiUtils::modelPath(parent, model_))) {
      expand(parent);
   }

   selectionModel()->select(parent.child(first, 0),
      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void OrdersView::onCollapsed(const QModelIndex &index)
{
   if (index.isValid()) {
      collapsed_.append(UiUtils::modelPath(index, model_));
   }
}

void OrdersView::onExpanded(const QModelIndex &index)
{
   if (index.isValid()) {
      collapsed_.removeOne(UiUtils::modelPath(index, model_));
   }
}
