
#ifndef RFQBLOTTERTREEVIEW_H_INCLUDED
#define RFQBLOTTERTREEVIEW_H_INCLUDED

#include "TreeViewWithEnterKey.h"


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

protected:
   void contextMenuEvent(QContextMenuEvent *e) override;

private:
   void setLimit(int limit);
}; // class RFQBlotterTreeView

#endif // RFQBLOTTERTREEVIEW_H_INCLUDED
