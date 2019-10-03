#include "OTCGlobalTable.h"
#include "OTCRequestViewModel.h"

OTCGlobalTable::OTCGlobalTable(QWidget* parent)
    : TreeViewWithEnterKey(parent)
{
}

void OTCGlobalTable::drawRow(QPainter* painter,const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   if (!index.isValid()) {
      TreeViewWithEnterKey::drawRow(painter, option, index);
      return;
   }

   const auto quotesModel = static_cast<OTCRequestViewModel*>(model());
   bool isOwnQuote = quotesModel->data(index, static_cast<int>(CustomRoles::OwnQuote)).toBool();

   QStyleOptionViewItem itemOption(option);
   if (isOwnQuote) {
      itemOption.palette.setColor(QPalette::Text, itemStyle_.colorUserOnline());
   }

   TreeViewWithEnterKey::drawRow(painter, itemOption, index);
}
