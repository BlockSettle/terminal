#include "WalletUtils.h"

#include <map>
#include <limits>

using namespace bs;

std::vector<UTXO> bs::selectUtxoForAmount(std::vector<UTXO> inputs, uint64_t amount)
{
   if (amount == std::numeric_limits<uint64_t>::max()) {
      return inputs;
   }

   std::map<int64_t, UTXO> inputsSorted;
   for (auto &utxo : inputs) {
      auto value = static_cast<int64_t>(utxo.getValue());
      inputsSorted.emplace(value, std::move(utxo));
   }

   auto remainingAmount = static_cast<int64_t>(amount);
   std::vector<UTXO> result;
   while (!inputsSorted.empty() && remainingAmount > 0) {
      auto it = inputsSorted.lower_bound(remainingAmount);
      if (it == inputsSorted.end()) {
         --it;
      }
      remainingAmount -= it->first;
      result.push_back(std::move(it->second));
      inputsSorted.erase(it);
   }

   return result;
}
