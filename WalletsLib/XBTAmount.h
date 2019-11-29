/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __XBT_AMOUNT_H__
#define __XBT_AMOUNT_H__

#include "BTCNumericTypes.h"

namespace bs {

// class XBTAmount should be used to unify getting satochi amount from double BTC
// across all codebase
// basically it is stronly typed wrapper for uin64_t
class XBTAmount
{
public:
   XBTAmount();
   explicit XBTAmount(const BTCNumericTypes::balance_type amount);
   explicit XBTAmount(const BTCNumericTypes::satoshi_type value);
   ~XBTAmount() noexcept = default;

   XBTAmount(const XBTAmount&) = default;
   XBTAmount& operator = (const XBTAmount&) = default;

   XBTAmount(XBTAmount&&) = default;
   XBTAmount& operator = (XBTAmount&&) = default;

   void SetValue(const BTCNumericTypes::balance_type amount);
   void SetValue(const BTCNumericTypes::satoshi_type value);

   BTCNumericTypes::satoshi_type GetValue() const;
   BTCNumericTypes::balance_type GetValueBitcoin() const;

   bool isZero() const;
private:
   static BTCNumericTypes::satoshi_type convertFromBitcoinToSatoshi(BTCNumericTypes::balance_type amount);
   static BTCNumericTypes::balance_type convertFromSatoshiToBitcoin(BTCNumericTypes::satoshi_type value);

private:
   BTCNumericTypes::satoshi_type value_;
};

}

#endif // __XBT_AMOUNT_H__
