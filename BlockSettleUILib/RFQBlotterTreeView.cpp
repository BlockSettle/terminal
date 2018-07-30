
#include "RFQBlotterTreeView.h"
#include "QuoteRequestsModel.h"

#include <QContextMenuEvent>
#include <QAbstractItemModel>
#include <QMenu>


//
// RFQBlotterTreeView
//

RFQBlotterTreeView::RFQBlotterTreeView(QWidget *parent)
   : TreeViewWithEnterKey (parent)
{
}

void RFQBlotterTreeView::contextMenuEvent(QContextMenuEvent *e)
{
   auto index = currentIndex();

   if (index.isValid()) {
      QMenu menu(this);
      QMenu limit(tr("Limit To"), &menu);
      limit.addAction(tr("1 RFQ"), [this] () { this->setLimit(1); });
      limit.addAction(tr("3 RFQs"), [this] () { this->setLimit(3); });
      limit.addAction(tr("5 RFQs"), [this] () { this->setLimit(5); });
      limit.addAction(tr("10 RFQs"), [this] () { this->setLimit(10); });
      limit.addAction(tr("25 RFQs"), [this] () { this->setLimit(25); });
      limit.addAction(tr("50 RFQs"), [this] () { this->setLimit(50); });
      menu.addMenu(&limit);

      if (index.data(static_cast<int>(QuoteRequestsModel::Role::LimitOfRfqs)).toInt() > 0) {
         menu.addSeparator();
         menu.addAction(tr("All RFQs"), [this] () { this->setLimit(-1); });
      }

      menu.exec(e->globalPos());
      e->accept();
   } else {
      e->ignore();
   }
}

void RFQBlotterTreeView::setLimit(int limit)
{
   auto index = this->currentIndex();

   while (index.parent().isValid()) {
      index = index.parent();
   }

   this->model()->setData(index, limit, static_cast<int>(QuoteRequestsModel::Role::LimitOfRfqs));
}
