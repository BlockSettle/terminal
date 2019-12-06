/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "RFQBlotterTreeView.h"
#include "QuoteRequestsModel.h"
#include "QuoteRequestsWidget.h"
#include "UiUtils.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QHeaderView>
#include <QPainter>


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

void RFQBlotterTreeView::setLimit(ApplicationSettings::Setting s, int limit)
{
   setLimit(findMarket(UiUtils::marketNameForLimit(s)), limit);
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

void RFQBlotterTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &option,
   const QModelIndex &index) const
{
   QTreeView::drawRow(painter, option, index);

   if(index.data(static_cast<int>(QuoteRequestsModel::Role::Type)).toInt() ==
      static_cast<int>(QuoteRequestsModel::DataType::Group)) {
      int left = header()->sectionViewportPosition(
         static_cast<int>(QuoteRequestsModel::Column::Product));
      int right = header()->sectionViewportPosition(
            static_cast<int>(QuoteRequestsModel::Column::Empty)) +
         header()->sectionSize(static_cast<int>(QuoteRequestsModel::Column::Empty));

      QRect r = option.rect;
      r.setX(left);
      r.setWidth(right - left);

      painter->save();
      painter->setFont(option.font);
      painter->setPen(QColor(0x00, 0xA9, 0xE3));

      if (isExpanded(index)) {
         painter->drawText(r, 0,
            index.data(static_cast<int>(QuoteRequestsModel::Role::StatText)).toString());
      } else {
         painter->drawText(r, 0,
            tr("0 of %1").arg(
               index.data(static_cast<int>(QuoteRequestsModel::Role::CountOfRfqs)).toInt()));
      }

      painter->restore();
   }
}

void RFQBlotterTreeView::setLimit(const QModelIndex &index, int limit)
{
   if (index.isValid()) {
      model_->limitRfqs(index, limit);
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

      setLimit(index, limit);
   }
}

QModelIndex RFQBlotterTreeView::findMarket(const QString &name) const
{
   return model_->findMarketIndex(name);
}
