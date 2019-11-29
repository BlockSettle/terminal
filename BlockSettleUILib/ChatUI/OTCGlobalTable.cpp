/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OTCGlobalTable.h"
#include "OTCRequestViewModel.h"
#include "OtcTypes.h"

namespace {
   const int kLeftOffset = 10;
}

OTCGlobalTable::OTCGlobalTable(QWidget* parent)
    : TreeViewWithEnterKey(parent)
{
   setItemDelegateForColumn(
      static_cast<int>(OTCRequestViewModel::Columns::Duration), new OTCRequestsProgressDelegate(this));
   setItemDelegateForColumn(
      static_cast<int>(OTCRequestViewModel::Columns::Security), new LeftOffsetDelegate(this));
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

bool OTCRequestsProgressDelegate::isDrawProgressBar(const QModelIndex& index) const
{
   return true;
}

int OTCRequestsProgressDelegate::maxValue(const QModelIndex& index) const
{
   return std::chrono::duration_cast<std::chrono::seconds>(
      bs::network::otc::publicRequestTimeout()).count();
}

int OTCRequestsProgressDelegate::currentValue(const QModelIndex& index) const
{
   QDateTime startTimeStamp = index.data(static_cast<int>(CustomRoles::RequestTimeStamp)).toDateTime();
   QDateTime endTimeStamp = startTimeStamp.addSecs(
      std::chrono::duration_cast<std::chrono::seconds>(
         bs::network::otc::publicRequestTimeout()).count());

   return QDateTime::currentDateTime().secsTo(endTimeStamp);
}

void LeftOffsetDelegate::paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
   QStyleOptionViewItem changedOpt = opt;
   changedOpt.rect.setLeft(changedOpt.rect.left() + kLeftOffset);

   QStyledItemDelegate::paint(painter, changedOpt, index);
}
