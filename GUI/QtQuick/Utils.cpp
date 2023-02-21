#include "Utils.h"
#include "BTCNumericTypes.h"

using namespace gui_utils;

QString gui_utils::satoshiToQString(double balance)
{
   if (balance < 0) {
      balance = 0;
   }
   return normalizedSatoshiToQString(balance / BTCNumericTypes::BalanceDivider);
}

QString gui_utils::normalizedSatoshiToQString(double balance)
{
   if (balance < 0) {
      balance = 0;
   }
   return QString::number(balance, 'f', 8);
}
