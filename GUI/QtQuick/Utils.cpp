#include "Utils.h"
#include "BTCNumericTypes.h"

using namespace gui_utils;

QString gui_utils::satoshiToQString(int64_t balance)
{
   return normalizedSatoshiToQString(balance / BTCNumericTypes::BalanceDivider);
}

QString gui_utils::normalizedSatoshiToQString(double balance)
{
   return QString::number(balance, 'f', 8);
}
