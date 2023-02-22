#include "Utils.h"
#include "BTCNumericTypes.h"

using namespace gui_utils;

QString gui_utils::satoshiToQString(int64_t balance)
{
   return xbtToQString(balance / BTCNumericTypes::BalanceDivider);
}

QString gui_utils::xbtToQString(double balance)
{
   return QString::number(balance, 'f', 8);
}
