#include "Utils.h"
#include "BTCNumericTypes.h"

using namespace gui_utils;

QString gui_utils::balance_to_qstring(float balance)
{
   if (balance < 0) {
      balance = 0;
   }
   return normalized_balance_to_qstring(balance / BTCNumericTypes::BalanceDivider);
}

QString gui_utils::normalized_balance_to_qstring(float balance)
{
   if (balance < 0) {
      balance = 0;
   }
   return QString::number(balance, 'f', 8);
}
