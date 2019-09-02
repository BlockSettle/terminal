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
