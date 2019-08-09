#include "SettlementMonitor.h"
#include "FastLock.h"
#include "CoinSelection.h"
#include "Wallets/SyncWallet.h"


bs::SettlementMonitor::SettlementMonitor(const std::shared_ptr<AsyncClient::BtcWallet> rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::core::SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger)
   : rtWallet_(rtWallet)
   , settlAddress_(addr->getPrefixedHash())
   , armoryPtr_(armory)
   , logger_(logger)
{
   init(armory.get());

   const auto ae = entryToAddress(addr);
   buyAuthKey_ = ae->buyChainedPubKey();
   sellAuthKey_ = ae->sellChainedPubKey();

   const auto &addrHashes = addr->getAsset()->supportedAddrHashes();
   ownAddresses_.insert(addrHashes.begin(), addrHashes.end());
}

bs::SettlementMonitor::SettlementMonitor(const std::shared_ptr<AsyncClient::BtcWallet> rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SettlementAddress> &addrEntry, const bs::Address &addr
   , const std::shared_ptr<spdlog::logger>& logger)
   : rtWallet_(rtWallet)
   , armoryPtr_(armory)
   , logger_(logger)
   , settlAddress_(addr)
   , buyAuthKey_(addrEntry->buyChainedPubKey())
   , sellAuthKey_(addrEntry->sellChainedPubKey())
{
   init(armory.get());

   const auto &addrHashes = addrEntry->supportedAddrHashes();
   ownAddresses_.insert(addrHashes.begin(), addrHashes.end());
}

bs::SettlementMonitor::SettlementMonitor(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<spdlog::logger> &logger, const bs::Address &addr
   , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey
   , const std::function<void()> &cbInited)
   : armoryPtr_(armory)
   , logger_(logger)
   , settlAddress_(addr)
   , buyAuthKey_(buyAuthKey)
   , sellAuthKey_(sellAuthKey)
{
   init(armory.get());

   const auto walletId = addr.display();
   rtWallet_ = armory_->instantiateWallet(walletId);
   const auto regId = armory_->registerWallet(rtWallet_, walletId, walletId
      , { addr.id() }, [cbInited](const std::string &) { cbInited(); });

   ownAddresses_.insert({ addr.unprefixed() });
}

void bs::SettlementMonitor::onNewBlock(unsigned int)
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

   const auto &cbHistory = [this](ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
      try {
         auto le = entries.get();
         if (le.empty()) {
            logger_->debug("[SettlementMonitor::checkNewEntries] empty history page for {}"
               , settlAddress_.display());
            return;
         } else {
            logger_->debug("[SettlementMonitor::checkNewEntries] get {} entries for {}"
               , le.size(), settlAddress_.display());
         }

         for (const auto &entry : le) {
            const auto &cbPayOut = [this, entry](bool ack) {
               if (ack) {
                  SendPayOutNotification(entry);
               }
               else {
                  logger_->error("[SettlementMonitor::checkNewEntries] not "
                                 "payin or payout transaction detected for "
                                 "settlement address {}", settlAddress_.display());
               }
            };
            const auto &cbPayIn = [this, entry, cbPayOut](bool ack) {
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
      catch (std::exception& e) {
         if(logger_ != nullptr) {
            logger_->error("[bs::SettlementMonitor::checkNewEntries] Return " \
               "data error - {}", e.what());
         }
      }

      {
         FastLock locker(walletLock_);
         if (!rtWallet_) {
            return;
         }
      }
   };
   {
      FastLock locker(walletLock_);
      if (!rtWallet_) {
         return;
      }
   }
   rtWallet_->getHistoryPage(0, cbHistory);  //XXX use only the first page for monitoring purposes
}

void bs::SettlementMonitor::IsPayInTransaction(const ClientClasses::LedgerEntry &entry
   , std::function<void(bool)> cb) const
{
   const auto &cbTX = [this, entry, cb](const Tx &tx) {
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
   const auto &cbTX = [this, entry, cb](const Tx &tx) {
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

      const auto &cbTXs = [this, txOutIdx, cb](const std::vector<Tx> &txs) {
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

      const auto &cbPayoutType = [this](bs::PayoutSigner::Type poType) {
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
   const auto &cbSpendable = [this, cb, allowZC]
      (ReturnMessage<std::vector<UTXO>> inputs) {
      try {
         auto inUTXOs = inputs.get();
         if (inUTXOs.empty()) {
            if (allowZC) {
               const auto &cbZC = [this, cb]
               (ReturnMessage<std::vector<UTXO>> zcs)->void {
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

   const auto &scriptRecipient = recvAddr.getRecipient(inputAmount);
   return coinSelection.getFeeForMaxVal(scriptRecipient->getSize(), feePerByte, { input });
}

bs::core::wallet::TXSignRequest bs::SettlementMonitor::createPayoutTXRequest(const UTXO &input
   , const bs::Address &recvAddr, float feePerByte, unsigned int topBlock)
{
   bs::core::wallet::TXSignRequest txReq;
   txReq.inputs.push_back(input);
   uint64_t fee = getEstimatedFeeFor(input, recvAddr, feePerByte, topBlock);

   if (fee < bs::sync::wallet::kMinRelayFee) {
      fee = bs::sync::wallet::kMinRelayFee;
   }

   uint64_t value = input.getValue();
   if (value < fee) {
      value = 0;
   } else {
      value = value - fee;
   }

   txReq.fee = fee;
   txReq.recipients.emplace_back(recvAddr.getRecipient(value));
   return txReq;
}

UTXO bs::SettlementMonitor::getInputFromTX(const bs::Address &addr
   , const BinaryData &payinHash, const double amount)
{
   const uint64_t value = amount * BTCNumericTypes::BalanceDivider;
   const uint32_t txHeight = UINT32_MAX;

   return UTXO(value, txHeight, 0, 0, payinHash
      , BtcUtils::getP2WSHOutputScript(addr.unprefixed()));
}


void bs::PayoutSigner::WhichSignature(const Tx& tx
   , uint64_t value
   , const bs::Address &settlAddr
   , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory, std::function<void(Type)> cb)
{
   if (!tx.isInitialized() || buyAuthKey.isNull() || sellAuthKey.isNull()) {
      cb(Failed);
      return;
   }

   struct Result {
      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::set<uint32_t>>  txOutIdx;
      uint64_t value;
   };
   auto result = std::make_shared<Result>();
   result->value = value;

   const auto &cbProcess = [result, settlAddr, buyAuthKey, sellAuthKey, tx, cb, logger]
      (const std::vector<Tx> &txs)
   {
      for (const auto &prevTx : txs) {
         const auto &txHash = prevTx.getThisHash();
         for (const auto &txOutIdx : result->txOutIdx[txHash]) {
            TxOut prevOut = prevTx.getTxOutCopy(txOutIdx);
            result->value += prevOut.getValue();
         }
         result->txHashSet.erase(txHash);
      }

      constexpr uint32_t txIndex = 0;
      constexpr uint32_t txOutIndex = 0;
      constexpr int inputId = 0;

      const TxIn in = tx.getTxInCopy(inputId);
      const OutPoint op = in.getOutPoint();
      const auto payinHash = op.getTxHash();

      UTXO utxo(result->value, UINT32_MAX, txIndex, txOutIndex, payinHash
         , BtcUtils::getP2WSHOutputScript(settlAddr.unprefixed()));

      //serialize signed tx
      auto txdata = tx.serialize();
      auto bctx = BCTX::parse(txdata);

      std::map<BinaryData, std::map<unsigned, UTXO>> utxoMap;

      utxoMap[utxo.getTxHash()][inputId] = utxo;

      //setup verifier
      try {
         TransactionVerifier tsv(*bctx, utxoMap);

         auto tsvFlags = tsv.getFlags();
         tsvFlags |= SCRIPT_VERIFY_P2SH_SHA256 | SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_SEGWIT;
         tsv.setFlags(tsvFlags);

         auto verifierState = tsv.evaluateState();

         auto inputState = verifierState.getSignedStateForInput(inputId);

         if (inputState.getSigCount() == 0) {
            logger->error("[bs::PayoutSigner::WhichSignature] no signatures received for TX: {}"
               , tx.getThisHash().toHexStr());
         }

         if (inputState.isSignedForPubKey(buyAuthKey)) {
            cb(SignedByBuyer);
         } else if (inputState.isSignedForPubKey(sellAuthKey)) {
            cb(SignedBySeller);
         } else {
            cb(SignatureUndefined);
         }
         return;
      } catch (const std::exception &e) {
         logger->error("[PayoutSigner::WhichSignature] failed: {}", e.what());
      }
      cb(Failed);
   };
   if (value == 0) {    // needs to be a sum of inputs in this case
      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         const OutPoint op = tx.getTxInCopy(i).getOutPoint();
         result->txHashSet.insert(op.getTxHash());
         result->txOutIdx[op.getTxHash()].insert(op.getTxOutIndex());
      }
      armory->getTXsByHash(result->txHashSet, cbProcess);
   } else {
      cbProcess({});
   }
}

std::shared_ptr<bs::SettlementAddress> bs::entryToAddress(
   const std::shared_ptr<bs::core::SettlementAddressEntry> &ae)
{
   return std::make_shared<bs::SettlementAddress>(ae->getAsset()->supportedAddrHashes()
      , ae->getAsset()->buyChainedPubKey(), ae->getAsset()->sellChainedPubKey());
}

void bs::SettlementMonitor::CheckPayoutSignature(const ClientClasses::LedgerEntry &entry
   , std::function<void(PayoutSigner::Type)> cb) const
{
   const auto amount = entry.getValue();
   const uint64_t value = amount < 0 ? -amount : amount;

   const auto &cbTX = [this, value, cb](const Tx &tx) {
      bs::PayoutSigner::WhichSignature(tx, value, settlAddress_, buyAuthKey_, sellAuthKey_
         , logger_, armoryPtr_, cb);
   };

   if (!armoryPtr_->getTxByHash(entry.getTxHash(), cbTX)) {
      logger_->error("[SettlementMonitor::CheckPayoutSignature] failed to get TX by hash");
   }
}

bs::SettlementMonitor::~SettlementMonitor() noexcept
{
   cleanup();
   FastLock locker(walletLock_);
   rtWallet_ = nullptr;
}


bs::SettlementMonitorQtSignals::SettlementMonitorQtSignals(const std::shared_ptr<AsyncClient::BtcWallet> rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::core::SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger)
   : SettlementMonitor(rtWallet, armory, addr, logger)
{}

bs::SettlementMonitorQtSignals::SettlementMonitorQtSignals(const std::shared_ptr<AsyncClient::BtcWallet> rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SettlementAddress> &addrEntry, const bs::Address &addr
   , const std::shared_ptr<spdlog::logger>& logger)
   : SettlementMonitor(rtWallet, armory, addrEntry, addr, logger)
{}

bs::SettlementMonitorQtSignals::~SettlementMonitorQtSignals() noexcept
{
   stop();
}

void bs::SettlementMonitorQtSignals::start()
{
   //armory_->addTarget(this);   // auto-added in constructor
   checkNewEntries();
}

void bs::SettlementMonitorQtSignals::stop()
{
   armory_->removeTarget(this);
}

void bs::SettlementMonitorQtSignals::onPayInDetected(int confirmationsNumber, const BinaryData &txHash)
{
   emit payInDetected(confirmationsNumber, txHash);
}

void bs::SettlementMonitorQtSignals::onPayOutDetected(int confirmationsNumber, PayoutSigner::Type signedBy)
{
   emit payOutDetected(confirmationsNumber, signedBy);
}

void bs::SettlementMonitorQtSignals::onPayOutConfirmed(PayoutSigner::Type signedBy)
{
   emit payOutConfirmed(signedBy);
}


bs::SettlementMonitorCb::SettlementMonitorCb(const std::shared_ptr<AsyncClient::BtcWallet> &rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::core::SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger)
 : SettlementMonitor(rtWallet, armory, addr, logger)
{}

bs::SettlementMonitorCb::SettlementMonitorCb(const std::shared_ptr<AsyncClient::BtcWallet> &rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SettlementAddress> &addrEntry, const bs::Address &addr
   , const std::shared_ptr<spdlog::logger>& logger)
   : SettlementMonitor(rtWallet, armory, addrEntry, addr, logger)
{}

bs::SettlementMonitorCb::~SettlementMonitorCb() noexcept
{
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

void bs::SettlementMonitorCb::onPayOutDetected(int confirmationsNumber, PayoutSigner::Type signedBy)
{
   if (onPayOutDetected_) {
      onPayOutDetected_(confirmationsNumber, signedBy);
   } else {
      logger_->error("[SettlementMonitorCb::onPayOutDetected] cb not set for {}"
         , settlAddress_.display());
   }
}

void bs::SettlementMonitorCb::onPayOutConfirmed(PayoutSigner::Type signedBy)
{
   if (onPayOutConfirmed_) {
      onPayOutConfirmed_(signedBy);
   } else {
      logger_->error("[SettlementMonitorCb::onPayOutConfirmed] cb not set for {}"
         , settlAddress_.display());
   }
}
