#ifndef WALLET_UTILS_H
#define WALLET_UTILS_H

#include "TxClasses.h"

namespace bs {

   // Try to select inputs so their sum is at least amount
   // Inputs selected in proper order so first used inputs that are big enough (but not too big)
   std::vector<UTXO> selectUtxoForAmount(std::vector<UTXO> inputs, uint64_t amount);

}

#endif

