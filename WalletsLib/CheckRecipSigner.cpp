#include "CheckRecipSigner.h"

#include "FastLock.h"
#include "PyBlockDataManager.h"

#include <spdlog/spdlog.h>

using namespace bs;


bool bs::TxAddressChecker::containsInputAddress(Tx tx, uint64_t lotsize, uint64_t value, unsigned int inputId)
{
   TxIn in = tx.getTxInCopy(inputId);
   if (!in.isInitialized()) {
      return false;
   }
   OutPoint op = in.getOutPoint();

   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return false;
   }
   const Tx prevTx = bdm->getTxByHash(op.getTxHash());
   if (prevTx.isInitialized()) {
      const TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
      const auto txAddr = bs::Address::fromTxOut(prevOut);
      const auto prevOutVal = prevOut.getValue();
      if ((txAddr.prefixed() == address_.prefixed()) && (value <= prevOutVal)) {
         return true;
      }
      if ((txAddr.getType() != AddressEntryType_P2PKH) && ((prevOutVal % lotsize) == 0)) {
         if (containsInputAddress(prevTx, lotsize, prevOutVal)) {
            return true;
         }
      }
   }
   return false;
}


bool CheckRecipSigner::findRecipAddress(const Address &address, cbFindRecip cb) const
{
   uint64_t value = 0;
   for (const auto &recipient : recipients_) {
      const auto recipientAddress = bs::CheckRecipSigner::getRecipientAddress(recipient);
      if (address == recipientAddress) {
         value += recipient->getValue();
      }
   }
   if (value) {
      if (cb) {
         cb(value);
      }
      return true;
   }
   return false;
}

struct recip_compare {
   bool operator() (const std::shared_ptr<ScriptRecipient> &a, const std::shared_ptr<ScriptRecipient> &b)
   {
      return (a->getSerializedScript() < b->getSerializedScript());
   }
};
void CheckRecipSigner::removeDupRecipients()
{  // can be implemented later in a better way without temporary std::set
   std::set<std::shared_ptr<ScriptRecipient>, recip_compare> recipSet(recipients_.begin(), recipients_.end());
   recipients_.clear();
   recipients_.insert(recipients_.end(), recipSet.begin(), recipSet.end());
}

bool CheckRecipSigner::hasInputAddress(const bs::Address &addr, uint64_t lotsize) const
{
   TxAddressChecker checker(addr);
   for (const auto &spender : spenders_) {
      auto outpoint = spender->getOutpoint();
      BinaryRefReader brr(outpoint);
      auto&& hash = brr.get_BinaryDataRef(32);

      const auto &tx = PyBlockDataManager::instance()->getTxByHash(hash);
      if (tx.isInitialized() && checker.containsInputAddress(tx, lotsize)) {
         return true;
      }
   }
   return false;
}

bool CheckRecipSigner::hasReceiver() const
{
   if (recipients_.empty()) {
      return false;
   }
   uint64_t inputVal = 0, outputVal = 0;
   for (const auto &spender : spenders_) {
      inputVal += spender->getValue();
   }
   for (const auto &recip : recipients_) {
      outputVal += recip->getValue();
   }
   return (inputVal == outputVal);
}

uint64_t CheckRecipSigner::estimateFee(float feePerByte) const
{
   size_t txSize = 0;
   unsigned int idMap = 0;
   if (!hasReceiver()) {
      txSize += 35;
   }
   else {
      for (const auto &recip : recipients_) {
         txSize += recip->getSize();
      }
   }

   for (const auto &spender : spenders_) {
      const auto &utxo = spender->getUtxo();
      const auto scrType = BtcUtils::getTxOutScriptType(utxo.getRecipientScrAddr());
      switch (scrType) {
      case TXOUT_SCRIPT_STDHASH160:
         txSize += 180;
         break;
      case TXOUT_SCRIPT_P2WPKH:
         txSize += 45;
         break;
      case TXOUT_SCRIPT_NONSTANDARD:
      case TXOUT_SCRIPT_P2SH:
      default:
         txSize += 90;
         break;
      }
   }

   return txSize * feePerByte;
}

uint64_t CheckRecipSigner::spendValue() const
{
   uint64_t result = 0;
   for (const auto &recip : recipients_) {
      result += recip->getValue();
   }
   return result;
}


vector<Address> CheckRecipSigner::GetInputAddressList(const std::shared_ptr<spdlog::logger>& logger) const
{
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      logger->error("[CheckRecipSigner::GetInputAddressList] no BDM connection");
      return {};
   }

   vector<Address > inputAddresses;

   for (const auto &spender : spenders_) {
      auto outputHash = spender->getOutputHash();
      if (outputHash.isNull() || outputHash.getSize() == 0) {
         logger->error("[CheckRecipSigner::GetInputAddressList] spender have empty output hash");
         return {};
      }

      const auto &tx = PyBlockDataManager::instance()->getTxByHash(outputHash);
      if (!tx.isInitialized()) {
         logger->error("[CheckRecipSigner::GetInputAddressList] 0x{} hash have uninitialized TX"
            , outputHash.toHexStr());
         return {};
      }

      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         TxIn in = tx.getTxInCopy(i);

         OutPoint op = in.getOutPoint();
         const Tx prevTx = bdm->getTxByHash(op.getTxHash());

         const TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());

         inputAddresses.emplace_back( Address{prevOut.getScrAddressStr()});
      }
   }

   return inputAddresses;
}


int TxChecker::receiverIndex(const bs::Address &addr) const
{
   if (!tx_.isInitialized()) {
      return -1;
   }

   for (size_t i = 0; i < tx_.getNumTxOut(); i++) {
      TxOut out = tx_.getTxOutCopy((int)i);
      if (!out.isInitialized()) {
         continue;
      }
      const auto &txAddr = bs::Address::fromTxOut(out);
      if (addr.id() == txAddr.id()) {
         return i;
      }
   }
   return -1;
}

bool TxChecker::hasReceiver(const bs::Address &addr) const
{
   return (receiverIndex(addr) >= 0);
}

bool TxChecker::hasSpender(const bs::Address &addr) const
{
   if (!tx_.isInitialized()) {
      return false;
   }
   for (size_t i = 0; i < tx_.getNumTxIn(); ++i) {
      TxIn in = tx_.getTxInCopy((int)i);
      if (!in.isInitialized()) {
         continue;
      }
      OutPoint op = in.getOutPoint();
      const auto &bdm = PyBlockDataManager::instance();
      if (!bdm) {
         return false;
      }
      const auto prevTx = bdm->getTxByHash(op.getTxHash());
      if (!prevTx.isInitialized()) {
         continue;
      }
      const TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
      const bs::Address &txAddr = bs::Address::fromTxOut(prevOut);
      if (txAddr.id() == addr.id()) {
         return true;
      }
   }
   return false;
}

bool TxChecker::hasInput(const BinaryData &txHash) const
{
   if (!tx_.isInitialized()) {
      return false;
   }
   for (size_t i = 0; i < tx_.getNumTxIn(); ++i) {
      TxIn in = tx_.getTxInCopy((int)i);
      if (!in.isInitialized()) {
         continue;
      }
      OutPoint op = in.getOutPoint();
      if (op.getTxHash() == txHash) {
         return true;
      }
   }
   return false;
}
