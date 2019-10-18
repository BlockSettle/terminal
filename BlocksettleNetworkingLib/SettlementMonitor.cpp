#include "SettlementMonitor.h"

#include "CoinSelection.h"
#include "FastLock.h"
#include "TradesVerification.h"
#include "Wallets/SyncWallet.h"

bs::SettlementMonitor::SettlementMonitor(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<spdlog::logger> &logger, const bs::Address &addr
   , const SecureBinaryData &buyAuthKey, const SecureBinaryData &sellAuthKey
   , const std::function<void()> &cbInited)
   : armoryPtr_(armory)
   , logger_(logger)
   , settlAddress_(addr)
   , buyAuthKey_(buyAuthKey)
   , sellAuthKey_(sellAuthKey)
{
   init(armory.get());

   ownAddresses_.insert({ addr.unprefixed() });

   const auto walletId = addr.display();
   rtWallet_ = armory_->instantiateWallet(walletId);
   const auto regId = armory_->registerWallet(rtWallet_, walletId, walletId
      , { addr.id() }
      , [cbInited](const std::string &) { cbInited(); });
}

void bs::SettlementMonitor::onNewBlock(unsigned int, unsigned int)
{
   checkNewEntries();
}

void bs::SettlementMonitor::onZCReceived(const std::vector<bs::TXEntry> &)
{
   checkNewEntries();
}

void bs::SettlementMonitor::checkNewEntries()
{
   logger_->debug("[SettlementMonitor::checkNewEntries] checking entries for {}"
      , settlAddress_.display());

   const auto &cbHistory = [this, handle = validityFlag_.handle()]
      (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries) mutable -> void
   {
      std::lock_guard<ValidityHandle> lock(handle);
      if (!handle.isValid()) {
         return;
      }

      try {
         auto le = entries.get();
         if (le.empty()) {
            return;
         } else {
            logger_->debug("[SettlementMonitor::checkNewEntries] get {} entries for {}"
               , le.size(), settlAddress_.display());
         }

         for (const auto &entry : le) {
            const auto &cbPayOut = [this, entry, handle](bool ack) mutable {
               std::lock_guard<ValidityHandle> lock(handle);
               if (!handle.isValid()) {
                  return;
               }

               if (ack) {
                  SendPayOutNotification(entry);
               }
               else {
                  logger_->error("[SettlementMonitor::checkNewEntries] not "
                                 "payin or payout transaction detected for "
                                 "settlement address {}", settlAddress_.display());
               }
            };
            const auto &cbPayIn = [this, entry, cbPayOut, handle](bool ack) mutable
            {
               std::lock_guard<ValidityHandle> lock(handle);
               if (!handle.isValid()) {
                  return;
               }

               if (ack) {
                  SendPayInNotification(armoryPtr_->getConfirmationsNumber(entry),
                                        entry.getTxHash());
               }
               else {
                  IsPayOutTransaction(entry, cbPayOut);
               }
            };
            IsPayInTransaction(entry, cbPayIn);
         }
      }
      catch (const std::exception &e) {
         if (logger_) {
            logger_->error("[bs::SettlementMonitor::checkNewEntries] failed to " \
               "get ledger entries: {}", e.what());
         }
      }
   };
   {
      FastLock locker(walletLock_);
      if (rtWallet_) {
         rtWallet_->getHistoryPage(0, cbHistory);  //XXX use only the first page for monitoring purposes
      }
   }
}

void bs::SettlementMonitor::IsPayInTransaction(const ClientClasses::LedgerEntry &entry
   , std::function<void(bool)> cb) const
{
   const auto &cbTX = [this, entry, cb, handle = validityFlag_.handle()]
      (const Tx &tx) mutable
   {
      ValidityGuard lock(handle);
      if (!handle.isValid()) {
         return;
      }

      if (!tx.isInitialized()) {
         logger_->error("[bs::SettlementMonitor::IsPayInTransaction] TX not initialized for {}."
            , entry.getTxHash().toHexStr());
         cb(false);
         return;
      }

      for (int i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy(i);
         const auto address = bs::Address::fromTxOut(out);
         if (ownAddresses_.find(address.unprefixed()) != ownAddresses_.end()) {
            cb(true);
            return;
         }
      }
      cb(false);
   };
   armoryPtr_->getTxByHash(entry.getTxHash(), cbTX);
}

void bs::SettlementMonitor::IsPayOutTransaction(const ClientClasses::LedgerEntry &entry
   , std::function<void(bool)> cb) const
{
   const auto &cbTX = [this, entry, cb, handle = validityFlag_.handle()]
      (const Tx &tx) mutable
   {
      ValidityGuard lock(handle);
      if (!handle.isValid()) {
         return;
      }

      if (!tx.isInitialized()) {
         logger_->error("[bs::SettlementMonitor::IsPayOutTransaction] TX not initialized for {}."
            , entry.getTxHash().toHexStr());
         cb(false);
         return;
      }
      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::set<uint32_t>> txOutIdx;

      for (int i = 0; i < tx.getNumTxIn(); ++i) {
         TxIn in = tx.getTxInCopy(i);
         OutPoint op = in.getOutPoint();

         txHashSet.insert(op.getTxHash());
         txOutIdx[op.getTxHash()].insert(op.getTxOutIndex());
      }

      const auto &cbTXs = [this, txOutIdx, cb, handle]
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

         for (const auto &prevTx : txs) {
            const auto &itIdx = txOutIdx.find(prevTx.getThisHash());
            if (itIdx == txOutIdx.end()) {
               continue;
            }
            for (const auto &txOutI : itIdx->second) {
               const TxOut prevOut = prevTx.getTxOutCopy(txOutI);
               const auto address = bs::Address::fromTxOut(prevOut);
               if (ownAddresses_.find(address.unprefixed()) != ownAddresses_.end()) {
                  cb(true);
                  return;
               }
            }
         }
         cb(false);
      };
      armoryPtr_->getTXsByHash(txHashSet, cbTXs);
   };
   armoryPtr_->getTxByHash(entry.getTxHash(), cbTX);
}

void bs::SettlementMonitor::SendPayInNotification(const int confirmationsNumber, const BinaryData &txHash)
{
   if ((confirmationsNumber != payinConfirmations_) && (!payinInBlockChain_)){

      logger_->debug("[SettlementMonitor::SendPayInNotification] payin detected for {}. Confirmations: {}"
            , settlAddress_.display(), confirmationsNumber);

      onPayInDetected(confirmationsNumber, txHash);

      payinInBlockChain_ = (confirmationsNumber != 0);
      payinConfirmations_ = confirmationsNumber;
   }
}

void bs::SettlementMonitor::SendPayOutNotification(const ClientClasses::LedgerEntry &entry)
{
   auto confirmationsNumber = armoryPtr_->getConfirmationsNumber(entry);
   if (payoutConfirmations_ != confirmationsNumber) {
      payoutConfirmations_ = confirmationsNumber;

      const auto &cbPayoutType = [this, handle = validityFlag_.handle()](bs::PayoutSignatureType poType) mutable {
         ValidityGuard lock(handle);
         if (!handle.isValid()) {
            return;
         }

         payoutSignedBy_ = poType;
         if (payoutConfirmations_ >= confirmedThreshold()) {
            if (!payoutConfirmedFlag_) {
               payoutConfirmedFlag_ = true;
               logger_->debug("[SettlementMonitor::SendPayOutNotification] confirmed payout for {}"
                  , settlAddress_.display());
               onPayOutConfirmed(payoutSignedBy_);
            }
         }
         else {
            logger_->debug("[SettlementMonitor::SendPayOutNotification] payout for {}. Confirmations: {}"
               , settlAddress_.display(), payoutConfirmations_);
            onPayOutDetected(payoutConfirmations_, payoutSignedBy_);
         }
      };
      CheckPayoutSignature(entry, cbPayoutType);
   }
}

void bs::SettlementMonitor::getPayinInput(const std::function<void(UTXO)> &cb
   , bool allowZC)
{
   const auto &cbSpendable = [this, cb, allowZC, handle = validityFlag_.handle()]
      (ReturnMessage<std::vector<UTXO>> inputs) mutable {
      ValidityGuard lock(handle);
      if (!handle.isValid()) {
         return;
      }

      try {
         auto inUTXOs = inputs.get();
         if (inUTXOs.empty()) {
            if (allowZC) {
               const auto &cbZC = [this, cb, handle]
               (ReturnMessage<std::vector<UTXO>> zcs) mutable -> void {
                  ValidityGuard lock(handle);
                  if (!handle.isValid()) {
                     return;
                  }

                  try {
                     auto inZCUTXOs = zcs.get();
                     if (inZCUTXOs.size() == 1) {
                        cb(inZCUTXOs[0]);
                     }
                     else {
                        cb({});
                     }
                  } catch (std::exception& e) {
                     if (logger_ != nullptr) {
                        logger_->error("[bs::SettlementWallet::GetInputFor] " \
                           "Return data error (getSpendableZCList) - {}",
                           e.what());
                     }
                  }
               };
               rtWallet_->getSpendableZCList(cbZC);
            }
         } else if (inUTXOs.size() == 1) {
            cb(inUTXOs[0]);
         }
         else {
            cb({});
         }
      } catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::SettlementWallet::GetInputFor] Return data " \
               "error (getSpendableTxOutListForValue) - {}", e.what());
         }
         cb({});
      }
   };
   rtWallet_->getSpendableTxOutListForValue(UINT64_MAX, cbSpendable);
}

uint64_t bs::SettlementMonitor::getEstimatedFeeFor(UTXO input, const bs::Address &recvAddr
   , float feePerByte, unsigned int topBlock)
{
   if (!input.isInitialized()) {
      return 0;
   }
   const auto inputAmount = input.getValue();
   if (input.txinRedeemSizeBytes_ == UINT32_MAX) {
      const bs::Address scrAddr(input.getRecipientScrAddr());
      input.txinRedeemSizeBytes_ = (unsigned int)scrAddr.getInputSize();
   }
   CoinSelection coinSelection([&input](uint64_t) -> std::vector<UTXO> { return { input }; }
   , std::vector<AddressBookEntry>{}, inputAmount, topBlock);

   const auto &scriptRecipient = recvAddr.getRecipient(bs::XBTAmount{ inputAmount });
   return coinSelection.getFeeForMaxVal(scriptRecipient->getSize(), feePerByte, { input });
}

bs::core::wallet::TXSignRequest bs::SettlementMonitor::createPayoutTXRequest(UTXO input
   , const bs::Address &recvAddr, float feePerByte, unsigned int topBlock)
{
   bs::core::wallet::TXSignRequest txReq;
   txReq.inputs.push_back(input);
   input.isInputSW_ = true;
   input.witnessDataSizeBytes_ = unsigned(bs::Address::getPayoutWitnessDataSize());
   uint64_t fee = getEstimatedFeeFor(input, recvAddr, feePerByte, topBlock);

   uint64_t value = input.getValue();
   if (value < fee) {
      value = 0;
   } else {
      value = value - fee;
   }

   txReq.fee = fee;
   txReq.recipients.emplace_back(recvAddr.getRecipient(bs::XBTAmount{ value }));
   return txReq;
}

UTXO bs::SettlementMonitor::getInputFromTX(const bs::Address &addr
   , const BinaryData &payinHash, const bs::XBTAmount& amount)
{
   constexpr uint32_t txHeight = UINT32_MAX;

   return UTXO(amount.GetValue(), txHeight, 0, 0, payinHash
      , BtcUtils::getP2WSHOutputScript(addr.unprefixed()));
}

void bs::PayoutSigner::WhichSignature(const Tx& tx
   , const bs::Address &settlAddr
   , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory
   , std::function<void(bs::PayoutSignatureType)> cb)
{
   if (!tx.isInitialized() || buyAuthKey.isNull() || sellAuthKey.isNull()) {
      cb(bs::PayoutSignatureType::Failed);
      return;
   }

   struct Result {
      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::set<uint32_t>>  txOutIdx;
   };
   auto result = std::make_shared<Result>();

   const auto cbProcess = [result, settlAddr, buyAuthKey, sellAuthKey, tx, cb, logger]
      (const std::vector<Tx> &txs, std::exception_ptr exPtr)
   {
      uint64_t value = 0;
      for (const auto &prevTx : txs) {
         const auto &txHash = prevTx.getThisHash();
         for (const auto &txOutIdx : result->txOutIdx[txHash]) {
            TxOut prevOut = prevTx.getTxOutCopy(txOutIdx);
            value += prevOut.getValue();
         }
         result->txHashSet.erase(txHash);
      }
      std::string errorMsg;
      auto result = TradesVerification::whichSignature(tx, value, settlAddr, buyAuthKey, sellAuthKey, &errorMsg);
      if (!errorMsg.empty()) {
         SPDLOG_LOGGER_ERROR(logger, "signature detection failed, errorMsg: '{}'", errorMsg);
      }
      cb(result);
   };

   // needs to be a sum of inputs in this case
   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      const OutPoint op = tx.getTxInCopy(i).getOutPoint();
      result->txHashSet.insert(op.getTxHash());
      result->txOutIdx[op.getTxHash()].insert(op.getTxOutIndex());
   }
   armory->getTXsByHash(result->txHashSet, cbProcess);
}

void bs::SettlementMonitor::CheckPayoutSignature(const ClientClasses::LedgerEntry &entry
   , std::function<void(bs::PayoutSignatureType)> cb) const
{
   const auto amount = entry.getValue();
   const uint64_t value = amount < 0 ? -amount : amount;

   const auto &cbTX = [this, value, cb](const Tx &tx) {
      std::string errorMsg;
      auto result = TradesVerification::whichSignature(tx, value, settlAddress_, buyAuthKey_, sellAuthKey_, &errorMsg);
      if (!errorMsg.empty()) {
         SPDLOG_LOGGER_ERROR(logger_, "signature detection failed: {}", errorMsg);
      }
      cb(result);
   };

   if (!armoryPtr_->getTxByHash(entry.getTxHash(), cbTX)) {
      logger_->error("[SettlementMonitor::CheckPayoutSignature] failed to get TX by hash");
   }
}

bs::SettlementMonitor::~SettlementMonitor() noexcept
{
   // Stop callbacks just in case (calling cleanup below should be enough)
   validityFlag_.reset();

   cleanup();
   FastLock locker(walletLock_);
   rtWallet_ = nullptr;
}

bs::SettlementMonitorCb::~SettlementMonitorCb() noexcept
{
   // Need to stop callbacks here because ValidityFlag destructor would be called too late
   validityFlag_.reset();

   stop();
}

void bs::SettlementMonitorCb::start(const onPayInDetectedCB& onPayInDetected
      , const onPayOutDetectedCB& onPayOutDetected
      , const onPayOutConfirmedCB& onPayOutConfirmed)
{
   onPayInDetected_ = onPayInDetected;
   onPayOutDetected_ = onPayOutDetected;
   onPayOutConfirmed_ = onPayOutConfirmed;

   checkNewEntries();
}

void bs::SettlementMonitorCb::stop()
{
   onPayInDetected_ = {};
   onPayOutDetected_ = {};
   onPayOutConfirmed_ = {};
}

void bs::SettlementMonitorCb::onPayInDetected(int confirmationsNumber, const BinaryData &txHash)
{
   if (onPayInDetected_) {
      onPayInDetected_(confirmationsNumber, txHash);
   } else {
      logger_->error("[SettlementMonitorCb::onPayInDetected] cb not set for {}"
         , settlAddress_.display());
   }
}

void bs::SettlementMonitorCb::onPayOutDetected(int confirmationsNumber, bs::PayoutSignatureType signedBy)
{
   if (onPayOutDetected_) {
      onPayOutDetected_(confirmationsNumber, signedBy);
   } else {
      logger_->error("[SettlementMonitorCb::onPayOutDetected] cb not set for {}"
         , settlAddress_.display());
   }
}

void bs::SettlementMonitorCb::onPayOutConfirmed(PayoutSignatureType signedBy)
{
   if (onPayOutConfirmed_) {
      onPayOutConfirmed_(signedBy);
   } else {
      logger_->error("[SettlementMonitorCb::onPayOutConfirmed] cb not set for {}"
         , settlAddress_.display());
   }
}
