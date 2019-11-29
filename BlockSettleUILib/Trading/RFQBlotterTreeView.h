/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef RFQBLOTTERTREEVIEW_H_INCLUDED
#define RFQBLOTTERTREEVIEW_H_INCLUDED

#include "TreeViewWithEnterKey.h"
#include "ApplicationSettings.h"

#include <memory>


class QuoteRequestsModel;
class QuoteReqSortModel;

//
// RFQBlotterTreeView
//

//! Tree view for RFQ blotter.
class RFQBlotterTreeView : public TreeViewWithEnterKey
{
   Q_OBJECT

public:
   RFQBlotterTreeView(QWidget *parent);
   ~RFQBlotterTreeView() noexcept override = default;

   void setRfqModel(QuoteRequestsModel *model);
   void setSortModel(QuoteReqSortModel *model);
   void setAppSettings(std::shared_ptr<ApplicationSettings> appSettings);

   void setLimit(ApplicationSettings::Setting s, int limit);

protected:
   void contextMenuEvent(QContextMenuEvent *e) override;
   void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
      const QModelIndex &index) const override;

private:
   void setLimit(const QModelIndex &index, int limit);
   void setLimit(int limit);
   QModelIndex findMarket(const QString &name) const;

private:
   QuoteRequestsModel * model_;
   QuoteReqSortModel *sortModel_;
   std::shared_ptr<ApplicationSettings> appSettings_;
}; // class RFQBlotterTreeView

#endif // RFQBLOTTERTREEVIEW_H_INCLUDED
