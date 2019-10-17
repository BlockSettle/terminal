#ifndef TRADES_VERIFICATION_H
#define TRADES_VERIFICATION_H

#include <cstdint>
#include <string>
#include <vector>

class BinaryData;
class UTXO;

namespace bs {

   class TradesVerification
   {
   public:

      struct Result
      {
         bool success{false};
         std::string errorMsg;

         // returns from verifyUnsignedPayin
         uint64_t totalFee{};
         uint64_t estimatedFee{};
         std::vector<UTXO> utxos;

         static Result error(std::string errorMsg);
      };

      static Result verifyUnsignedPayin(const BinaryData &unsignedPayin
         , float feePerByte, const std::string &settlementAddress, uint64_t tradeAmount);

   };

}

#endif
