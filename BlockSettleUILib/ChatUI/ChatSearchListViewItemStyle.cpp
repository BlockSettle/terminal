/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatSearchListViewItemStyle.h"

ChatSearchListViewItemStyle::ChatSearchListViewItemStyle(QWidget *parent)
   : QWidget(parent)
   , colorContactUnknown_(Qt::gray)
   , colorContactAccepted_(Qt::cyan)
   , colorContactIncoming_(Qt::darkYellow)
   , colorContactOutgoing_(Qt::darkGreen)
   , colorContactRejected_(Qt::red)
{
}
