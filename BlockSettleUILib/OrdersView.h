
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

protected:
   void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
      const QModelIndex &index) const override;

private slots:
   void onSelectRow(const QPersistentModelIndex &row);
   void onRowsInserted(const QModelIndex &parent, int, int);
   void onCollapsed(const QModelIndex &index);
   void onExpanded(const QModelIndex &index);

private:
   void setHasNewItemFlag(const QModelIndex &index, bool value);

private:
   QStringList collapsed_;
   OrderListModel *model_;
   std::map<QPersistentModelIndex, bool> hasNewItems_;
}; // class OrdersView

#endif // ORDERSVIEW_H_INCLUDED
