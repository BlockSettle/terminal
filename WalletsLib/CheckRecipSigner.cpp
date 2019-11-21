#include "CheckRecipSigner.h"

#include "ArmoryConnection.h"
#include "CoinSelection.h"
#include "FastLock.h"
#include "NetworkConfig.h"

using namespace bs;


void bs::TxAddressChecker::containsInputAddress(Tx tx, std::function<void(bool)> cb
   , uint64_t lotsize, uint64_t value, unsigned int inputId)
{
   if (!tx.isInitialized()) {
      cb(false);
      return;
   }
   TxIn in = tx.getTxInCopy(inputId);
   if (!in.isInitialized() || !lotsize) {
      cb(false);
      return;
   }
   OutPoint op = in.getOutPoint();

   const auto &cbTX = [this, op, cb, lotsize, value](const Tx &prevTx) {
      if (!prevTx.isInitialized()) {
         cb(false);
         return;
      }
      const TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
      const auto txAddr = bs::Address::fromTxOut(prevOut);
      const auto prevOutVal = prevOut.getValue();
      if ((txAddr.prefixed() == address_.prefixed()) && (value <= prevOutVal)) {
         cb(true);
         return;
      }
      if ((txAddr.getType() != AddressEntryType_P2PKH) && ((prevOutVal % lotsize) == 0)) {
         containsInputAddress(prevTx, cb, lotsize, prevOutVal);
      }
      else {
         cb(false);
      }
   };

   if (!armory_) {
      cb(false);
      return;
   }

   if (!armory_->getTxByHash(op.getTxHash(), cbTX)) {
      cb(false);
   }
}


bool CheckRecipSigner::findRecipAddress(const Address &address, cbFindRecip cb) const
{
   uint64_t valOutput = 0, valReturn = 0, valInput = 0;
   for (const auto &recipient : recipients_) {
      const auto recipientAddress = bs::CheckRecipSigner::getRecipientAddress(recipient);
      if (address == recipientAddress) {
         valOutput += recipient->getValue();
      }
      else {
         valReturn += recipient->getValue();
      }
   }
   for (const auto &spender : spenders_) {
      valInput += spender->getValue();
   }
   if (valOutput) {
      if (cb) {
         cb(valOutput, valReturn, valInput);
      }
      return true;
   }
   return false;
}

struct recip_compare {
   bool operator() (const std::shared_ptr<ScriptRecipient> &a, const std::shared_ptr<ScriptRecipient> &b) const
   {
      return (a->getSerializedScript() < b->getSerializedScript());
   }
};
void CheckRecipSigner::removeDupRecipients()
{  // can be implemented later in a better way without temporary std::set
   std::vector<std::shared_ptr<ScriptRecipient>> uniqueRecepients;

   std::set<std::shared_ptr<ScriptRecipient>, recip_compare> recipSet;
   for (const auto r : recipients_) {
      auto it = recipSet.find(r);
      if (it != recipSet.end()) {
         continue;
      }

      uniqueRecepients.emplace_back(r);
      recipSet.emplace(r);
   }

   recipients_.swap(uniqueRecepients);
}

void CheckRecipSigner::hasInputAddress(const bs::Address &addr, std::function<void(bool)> cb, uint64_t lotsize)
{
   if (!armory_) {
      cb(false);
      return;
   }
   for (const auto &spender : spenders_) {
      auto outpoint = spender->getOutpoint();
      BinaryRefReader brr(outpoint);
      auto&& hash = brr.get_BinaryDataRef(32);
      txHashSet_.insert(hash);
   }
   auto checker = std::make_shared<TxAddressChecker>(addr, armory_);
   resultFound_ = false;

   const auto &cbTXs = [this, cb, checker, lotsize, handle = validityFlag_.handle()]
      (const std::vector<Tx> &txs, std::exception_ptr exPtr) mutable
   {
      ValidityGuard lock(handle);
      if (!handle.isValid()) {
         return;
      }
      if (exPtr != nullptr) {
         cb(false);
         return;
      }
      for (const auto &tx : txs) {
         const auto &cbContains = [this, cb, tx, checker, handle](bool contains) mutable {
            ValidityGuard lock(handle);
            if (!handle.isValid()) {
               return;
            }
            if (resultFound_) {
               return;
            }
            txHashSet_.erase(tx.getThisHash());
            if (contains) {
               resultFound_ = true;
               cb(true);
               return;
            }
            if (txHashSet_.empty()) {
               resultFound_ = true;
               cb(false);
            }
         };
         if (!resultFound_) {
            checker->containsInputAddress(tx, cbContains, lotsize);
         }
      }
   };
   if (txHashSet_.empty()) {
      cb(false);
   }
   else {
      armory_->getTXsByHash(txHashSet_, cbTXs);
   }
}

bool CheckRecipSigner::hasReceiver() const
{
   return !recipients_.empty();
}

uint64_t CheckRecipSigner::estimateFee(float &feePerByte, uint64_t fixedFee) const
{
   size_t txSize = 0;
   std::vector<UTXO> inputs;
   inputs.reserve(spenders_.size());
   for (const auto &spender : spenders_) {
      inputs.emplace_back(std::move(spender->getUtxo()));
   }
   auto transactions = bs::Address::decorateUTXOsCopy(inputs);
   std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipientsMap;
   if (recipients_.empty()) {
      recipientsMap[0] = std::make_shared<Recipient_OPRETURN>(BinaryData("fake recipient"));
   }
   else {
      for (unsigned int i = 0; i < recipients_.size(); ++i) {
         recipientsMap[i] = recipients_[i];
      }
   }

   try {
      PaymentStruct payment(recipientsMap, fixedFee, 0, 0);

      auto usedUTXOCopy{ transactions };
      UtxoSelection selection{ usedUTXOCopy };
      selection.computeSizeAndFee(payment);

      feePerByte = selection.fee_byte_;

      const size_t nonWitSize = selection.size_ - selection.witnessSize_;
      txSize = std::ceil(static_cast<float>(3 * nonWitSize + selection.size_) / 4.0f);
      if (recipients_.empty()) { // subtract fake recipient size
         txSize -= recipientsMap[0]->getSize();
      }
   } catch (...) {}
   return txSize * feePerByte;
}

uint64_t CheckRecipSigner::outputsTotalValue() const
{
   uint64_t result = 0;
   for (const auto &recip : recipients_) {
      result += recip->getValue();
   }
   return result;
}

uint64_t CheckRecipSigner::inputsTotalValue() const
{
   uint64_t result = 0;
   for (const auto &spender : spenders_) {
      result += spender->getValue();
   }
   return result;
}

bool CheckRecipSigner::isRBF() const
{
   for (const auto &spender : spenders()) {
      if (spender->getSequence() < (UINT32_MAX - 1)) {
         return true;
      }
   }
   return false;
}

bool CheckRecipSigner::GetInputAddressList(const std::shared_ptr<spdlog::logger> &logger
   , std::function<void(std::vector<bs::Address>)> cb)
{
   auto result = std::make_shared<std::vector<Address>>();

   if (!armory_) {
      logger->error("[CheckRecipSigner::GetInputAddressList] there is no armory connection");
      return false;
   }

   const auto &cbTXs = [this, result, cb, handle = validityFlag_.handle()]
      (const std::vector<Tx> &txs, std::exception_ptr exPtr) mutable
   {
      ValidityGuard lock(handle);
      if (!handle.isValid()) {
         return;
      }
      if (exPtr != nullptr) {
         if (cb) {
            cb({});
         }
         return;
      }
      for (const auto &tx : txs) {
         if (!result) {
            return;
         }
         const auto &txHash = tx.getThisHash();
         txHashSet_.erase(txHash);
         for (const auto &txOutIdx : txOutIdx_[txHash]) {
            const TxOut prevOut = tx.getTxOutCopy(txOutIdx);
            result->emplace_back(bs::Address::fromHash(prevOut.getScrAddressStr()));
         }
         if (txHashSet_.empty()) {
            txOutIdx_.clear();
            cb(*result.get());
         }
      }
   };
   const auto &cbOutputTXs = [this, cbTXs, cb, handle = validityFlag_.handle()]
      (const std::vector<Tx> &txs, std::exception_ptr exPtr) mutable
   {
      ValidityGuard lock(handle);
      if (!handle.isValid()) {
         return;
      }
      if (exPtr != nullptr) {
         cb({});
         return;
      }
      for (const auto &tx : txs) {
         for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
            TxIn in = tx.getTxInCopy((int)i);
            OutPoint op = in.getOutPoint();
            txHashSet_.insert(op.getTxHash());
            txOutIdx_[op.getTxHash()].insert(op.getTxOutIndex());
         }
      }
      if (txHashSet_.empty()) {
         cb({});
      }
      else {
         armory_->getTXsByHash(txHashSet_, cbTXs);
      }
   };

   std::set<BinaryData> outputHashSet;
   txHashSet_.clear();
   for (const auto &spender : spenders_) {
      auto outputHash = spender->getOutputHash();
      if (outputHash.isNull() || outputHash.getSize() == 0) {
         logger->warn("[CheckRecipSigner::GetInputAddressList] spender has empty output hash");
      }
      else {
         outputHashSet.emplace(std::move(outputHash));
      }
   }
   if (outputHashSet.empty()) {
      cb({});
      return false;
   }
   else {
      armory_->getTXsByHash(outputHashSet, cbOutputTXs);
   }
   return true;
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
         return (int)i;
      }
   }
   return -1;
}

bool TxChecker::hasReceiver(const bs::Address &addr) const
{
   return (receiverIndex(addr) >= 0);
}

void TxChecker::hasSpender(const bs::Address &addr, const std::shared_ptr<ArmoryConnection> &armory
   , std::function<void(bool)> cb) const
{
   if (!tx_.isInitialized()) {
      cb(false);
      return;
   }

   struct Result {
      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::unordered_set<uint32_t>> txOutIdx;
   };
   auto result = std::make_shared<Result>();

   const auto &cbTXs = [result, addr, cb]
      (const std::vector<Tx> &txs, std::exception_ptr)
   {
      for (const auto &tx : txs) {
         for (const auto &txOutIdx : result->txOutIdx[tx.getThisHash()]) {
            const TxOut prevOut = tx.getTxOutCopy(txOutIdx);
            const bs::Address &txAddr = bs::Address::fromTxOut(prevOut);
            if (txAddr.id() == addr.id()) {
                cb(true);
                return;
            }
         }
      }
      cb(false);
   };

   for (size_t i = 0; i < tx_.getNumTxIn(); ++i) {
      TxIn in = tx_.getTxInCopy((int)i);
      if (!in.isInitialized()) {
         continue;
      }
      OutPoint op = in.getOutPoint();
      result->txHashSet.insert(op.getTxHash());
      result->txOutIdx[op.getTxHash()].insert(op.getTxOutIndex());
   }
   if (result->txHashSet.empty()) {
      cb(false);
   }
   else {
      armory->getTXsByHash(result->txHashSet, cbTXs);
   }
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


NetworkType getNetworkType()
{
   NetworkConfig config;
   switch (config.getMode()) {
   case NETWORK_MODE_MAINNET: return NetworkType::MainNet;
   case NETWORK_MODE_TESTNET: return NetworkType::TestNet;
   case NETWORK_MODE_REGTEST: return NetworkType::RegTest;
   default:       return NetworkType::RegTest;
   }
}
