#ifndef __BTC_NUMERIC_TYPES_H__
#define __BTC_NUMERIC_TYPES_H__

namespace BTCNumericTypes
{
   using balance_type = double;
   constexpr int default_precision = 8;

   const balance_type  BalanceDivider = 100000000;
}

#endif // __BTC_NUMERIC_TYPES_H__
