#ifndef __CHECK_RECIP_SIGNER_H__
#define __CHECK_RECIP_SIGNER_H__

#include "Address.h"
#include "BinaryData.h"
#include "Signer.h"
#include "TxClasses.h"

#include <memory>

namespace spdlog {
   class logger;
};


namespace bs {
   class TxAddressChecker
   {
   public:
      TxAddressChecker(const bs::Address &addr) : address_(addr) {}
      bool containsInputAddress(Tx, uint64_t lotsize = 1, uint64_t value = 0, unsigned int inputId = 0);

   private:
      const bs::Address                address_;
   };


   class CheckRecipSigner : public Signer
   {
   public:
      CheckRecipSigner() : Signer() {}
      CheckRecipSigner(const BinaryData bd) : Signer() {
         deserializeState(bd);
      }

      using cbFindRecip = std::function<void(uint64_t)>;
      bool findRecipAddress(const Address &address, cbFindRecip cb) const;

      bool hasInputAddress(const Address &, uint64_t lotsize = 1) const;
      uint64_t estimateFee(float feePerByte) const;
      uint64_t spendValue() const;

      vector<Address> GetInputAddressList(const std::shared_ptr<spdlog::logger>& logger) const;

      void removeDupRecipients();

      static bs::Address getRecipientAddress(const std::shared_ptr<ScriptRecipient> &recip) {
         return bs::Address::fromTxOutScript(getRecipientScriptAddr(recip));
      }

   private:
      static BinaryData getRecipientScriptAddr(const std::shared_ptr<ScriptRecipient> &recip) {
         const auto &recipScr = recip->getSerializedScript();
         const auto scr = recipScr.getSliceRef(8, recipScr.getSize() - 8);
         if (scr.getSize() != (size_t)(scr[0] + 1)) {
            return{};
         }
         return scr.getSliceCopy(1, scr[0]);
      }

      bool hasReceiver() const;
   };


   class TxChecker
   {
   public:
      TxChecker(const Tx &tx) : tx_(tx) {}

      int receiverIndex(const bs::Address &) const;
      bool hasReceiver(const bs::Address &) const;
      bool hasSpender(const bs::Address &) const;
      bool hasInput(const BinaryData &txHash) const;

   private:
      const Tx tx_;
   };

}  //namespace bs

#endif //__CHECK_RECIP_SIGNER_H__
