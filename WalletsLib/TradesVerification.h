#ifndef TRADES_VERIFICATION_H
#define TRADES_VERIFICATION_H

#include <cstdint>
#include <string>
#include <vector>

class BinaryData;
class Tx;
class UTXO;

namespace bs {

   class Address;

   enum class PayoutSignatureType : int
   {
      Undefined,
      ByBuyer,
      BySeller,
      Failed
   };

   const char *toString(const PayoutSignatureType t);

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
         int totalInputCount{};
         std::vector<UTXO> utxos;

         // returns from verifySignedPayout
         std::string payoutTxHashHex;

         static Result error(std::string errorMsg);
      };

      static bs::Address constructSettlementAddress(const BinaryData &settlementId
         , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey);

      static PayoutSignatureType whichSignature(const Tx &tx
         , uint64_t value
         , const bs::Address &settlAddr
         , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey, std::string *errorMsg = nullptr);

      static Result verifyUnsignedPayin(const BinaryData &unsignedPayin
         , float feePerByte, const std::string &settlementAddress, uint64_t tradeAmount);

      static Result verifySignedPayout(const BinaryData &signedPayout
         , const std::string &buyAuthKeyHex, const std::string &sellAuthKeyHex,  const BinaryData &payinHash
         , uint64_t tradeAmount, float feePerByte, const std::string &settlementId, const std::string &settlementAddress);

      static Result verifySignedPayin(const BinaryData &signedPayin, const BinaryData &payinHash, float feePerByte, uint64_t totalPayinFee);

   };

}

#endif
