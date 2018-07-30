
#ifndef RFQBLOTTERTREEVIEW_H_INCLUDED
#define RFQBLOTTERTREEVIEW_H_INCLUDED

#include "TreeViewWithEnterKey.h"


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

protected:
   void contextMenuEvent(QContextMenuEvent *e) override;

private:
   void setLimit(int limit);

private:
   QuoteRequestsModel * model_;
   QuoteReqSortModel *sortModel_;
}; // class RFQBlotterTreeView

#endif // RFQBLOTTERTREEVIEW_H_INCLUDED
