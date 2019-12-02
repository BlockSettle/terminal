/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BTC_NUMERIC_TYPES_H__
#define __BTC_NUMERIC_TYPES_H__

#include <cstdint>

namespace BTCNumericTypes
{
   using balance_type = double;
   using satoshi_type = uint64_t;
   constexpr int default_precision = 8;

   const balance_type  BalanceDivider = 100000000;
}

#endif // __BTC_NUMERIC_TYPES_H__
