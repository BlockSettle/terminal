#include "TradesVerification.h"

#include <spdlog/spdlog.h>

#include "BinaryData.h"
#include "CheckRecipSigner.h"

bs::TradesVerification::Result bs::TradesVerification::Result::error(std::string errorMsg)
{
   Result result;
   result.errorMsg = std::move(errorMsg);
   return result;
}

bs::TradesVerification::Result bs::TradesVerification::verifyUnsignedPayin(const BinaryData &unsignedPayin
   , float feePerByte, const std::string &settlementAddress, uint64_t tradeAmount
   )
{
   if (unsignedPayin.isNull()) {
      return Result::error("no unsigned payin provided");
   }

   try {
      bs::CheckRecipSigner deserializedSigner(unsignedPayin);

      // check that there is only one output of correct amount to settlement address
      auto recipients = deserializedSigner.recipients();
      uint64_t settlementAmount = 0;
      uint64_t totalOutputAmount = 0;
      uint64_t settlementOutputsCount = 0;

      for (const auto& recipient : recipients) {
         uint64_t value = recipient->getValue();

         totalOutputAmount += value;
         if (bs::CheckRecipSigner::getRecipientAddress(recipient).display() == settlementAddress) {
            settlementAmount += value;
            settlementOutputsCount += 1;
         }
      }

      if (settlementOutputsCount != 1) {
         return Result::error(fmt::format("unexpected settlement outputs count: {}. expected 1", settlementOutputsCount));
      }


      if (settlementAmount != tradeAmount) {
         return Result::error(fmt::format("unexpected settlement amount: {}. expected {}", settlementAmount, tradeAmount));
      }

      // check that fee is fine
      auto spenders = deserializedSigner.spenders();

      uint64_t totalInput = 0;
      for (const auto& spender : spenders) {
         totalInput += spender->getValue();
      }

      if (totalInput < totalOutputAmount) {
         return Result::error(fmt::format("total inputs {} lower that outputs {}", totalInput, totalOutputAmount));
      }

      // is not RBF
      Tx deserializedTx(unsignedPayin);

      if (deserializedTx.isRBF()) {
         return Result::error("payin could not be RBF transaction");
      }

      Result result;
      result.success = true;
      result.totalFee = totalInput - totalOutputAmount;
      result.estimatedFee = static_cast<uint64_t>(feePerByte * unsignedPayin.getSize());
      for (const auto& spender : spenders) {
         result.utxos.push_back(spender->getUtxo());
      }

      return result;

   } catch (const std::exception &e) {
      return Result::error(fmt::format("exception during payin processing: {}", e.what()));
   } catch (...) {
      return Result::error(fmt::format("undefined exception during payin processing"));
   }
}
