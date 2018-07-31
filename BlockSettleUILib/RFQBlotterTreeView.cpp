
#include "RFQBlotterTreeView.h"
#include "QuoteRequestsModel.h"
#include "QuoteRequestsWidget.h"
#include "UiUtils.h"

#include <QContextMenuEvent>
#include <QAbstractItemModel>
#include <QMenu>


//
// RFQBlotterTreeView
//

RFQBlotterTreeView::RFQBlotterTreeView(QWidget *parent)
   : TreeViewWithEnterKey (parent)
   , model_(nullptr)
   , sortModel_(nullptr)
{
}

void RFQBlotterTreeView::setRfqModel(QuoteRequestsModel *model)
{
   model_ = model;
}

void RFQBlotterTreeView::setSortModel(QuoteReqSortModel *model)
{
   sortModel_ = model;
}

void RFQBlotterTreeView::setAppSettings(std::shared_ptr<ApplicationSettings> appSettings)
{
   appSettings_ = appSettings;
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
   auto index = sortModel_->mapToSource(currentIndex());

   if (index.isValid()) {
      while (index.parent().isValid()) {
         index = index.parent();
      }

      appSettings_->set(UiUtils::limitRfqSetting(index.data().toString()), limit);

      model_->limitRfqs(index, limit);
   }
}
