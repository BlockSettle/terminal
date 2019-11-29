/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CURRENCY_PAIR_H__
#define __CURRENCY_PAIR_H__

#include <string>

class CurrencyPair
{
public:
   CurrencyPair(const std::string& pairString);

   const std::string &NumCurrency() const { return numCurrency_; }
   const std::string &DenomCurrency() const { return denomCurrency_; }
   const std::string &ContraCurrency(const std::string &cur);

private:
   std::string numCurrency_;
   std::string denomCurrency_;
   std::string invalidCurrency_;
};

#endif // __CURRENCY_PAIR_H__
