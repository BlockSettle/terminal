/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CurrencyPair.h"
#include <stdexcept>


CurrencyPair::CurrencyPair(const std::string& pairString)
{
   size_t pos = pairString.find('/');
   if (pos == std::string::npos) {
      throw std::runtime_error("no delimiter in definition");
   }

   numCurrency_ = pairString.substr(0, pos);
   denomCurrency_ = pairString.substr(pos + 1);
}

const std::string &CurrencyPair::ContraCurrency(const std::string &cur)
{
   if (cur == numCurrency_) {
      return denomCurrency_;
   }
   else if (cur == denomCurrency_) {
      return numCurrency_;
   }
   return invalidCurrency_;
}
