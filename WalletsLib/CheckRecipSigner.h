#ifndef __CHECK_RECIP_SIGNER_H__
#define __CHECK_RECIP_SIGNER_H__

#include "Address.h"
#include "BinaryData.h"
#include "BtcDefinitions.h"
#include "Signer.h"
#include "TxClasses.h"

#include <functional>
#include <memory>

namespace spdlog {
   class logger;
}
class ArmoryConnection;


namespace bs {
   class TxAddressChecker
   {
   public:
      TxAddressChecker(const bs::Address &addr, const std::shared_ptr<ArmoryConnection> &armory)
         : address_(addr), armory_(armory) {}
      void setArmory(const std::shared_ptr<ArmoryConnection> &armory) { armory_ = armory; }
      void containsInputAddress(Tx, std::function<void(bool)>
         , uint64_t lotsize = 1, uint64_t value = 0, unsigned int inputId = 0);

   private:
      const bs::Address                   address_;
      std::shared_ptr<ArmoryConnection>   armory_;
   };


   class CheckRecipSigner : public Signer
   {
   public:
      CheckRecipSigner(const std::shared_ptr<ArmoryConnection> &armory = nullptr)
         : Signer(), armory_(armory) {}
      CheckRecipSigner(const BinaryData bd, const std::shared_ptr<ArmoryConnection> &armory = nullptr)
         : Signer(), armory_(armory) {
         deserializeState(bd);
      }

      using cbFindRecip = std::function<void(uint64_t valOutput, uint64_t valReturn, uint64_t valInput)>;
      bool findRecipAddress(const Address &address, cbFindRecip cb) const;

      void hasInputAddress(const Address &, std::function<void(bool)>, uint64_t lotsize = 1);
      uint64_t estimateFee(float feePerByte) const;
      uint64_t spendValue() const;
      std::vector<std::shared_ptr<ScriptSpender>> spenders() const { return spenders_; }
      std::vector<std::shared_ptr<ScriptRecipient>> recipients() const { return recipients_; }

      bool GetInputAddressList(const std::shared_ptr<spdlog::logger> &logger, std::function<void(std::vector<bs::Address>)>);

      void removeDupRecipients();

      static bs::Address getRecipientAddress(const std::shared_ptr<ScriptRecipient> &recip) {
         return bs::Address::fromScript(getRecipientOutputScript(recip));
      }

   private:
      static BinaryData getRecipientOutputScript(const std::shared_ptr<ScriptRecipient> &recip) {
         const auto &recipScr = recip->getSerializedScript();
         const auto scr = recipScr.getSliceRef(8, (uint32_t)recipScr.getSize() - 8);
         if (scr.getSize() != (size_t)(scr[0] + 1)) {
            return{};
         }
         return scr.getSliceCopy(1, scr[0]);
      }

      bool hasReceiver() const;

   private:
      std::shared_ptr<ArmoryConnection>   armory_;
      std::set<BinaryData> txHashSet_;
      std::map<BinaryData, std::set<uint32_t>>  txOutIdx_;
      std::atomic_bool     resultFound_;
   };


   class TxChecker
   {
   public:
      TxChecker(const Tx &tx) : tx_(tx) {}

      int receiverIndex(const bs::Address &) const;
      bool hasReceiver(const bs::Address &) const;
      void hasSpender(const bs::Address &, const std::shared_ptr<ArmoryConnection> &
         , std::function<void(bool)>) const;
      bool hasInput(const BinaryData &txHash) const;

   private:
      const Tx tx_;
   };

}  //namespace bs

NetworkType getNetworkType();

#endif //__CHECK_RECIP_SIGNER_H__
