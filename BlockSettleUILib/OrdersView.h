
#ifndef ORDERSVIEW_H_INCLUDED
#define ORDERSVIEW_H_INCLUDED

#include "TreeViewWithEnterKey.h"


class OrderListModel;


//
// OrdersView
//

//! View for orders.
class OrdersView : public TreeViewWithEnterKey
{
   Q_OBJECT

public:
   explicit OrdersView(QWidget *parent);
   ~OrdersView() noexcept override = default;

   void initWithModel(OrderListModel *model);

private slots:
   void onRowsInserted(const QModelIndex &parent, int, int);
   void onCollapsed(const QModelIndex &index);
   void onExpanded(const QModelIndex &index);

private:
   QStringList collapsed_;
   OrderListModel *model_;
}; // class OrdersView

#endif // ORDERSVIEW_H_INCLUDED
