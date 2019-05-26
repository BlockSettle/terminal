#include "SyncWallet.h"

#include <QLocale>
#include <QMutexLocker>
#include <bech32/ref/c++/segwit_addr.h>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "CoinSelection.h"
#include "SignContainer.h"

using namespace bs::sync;

Wallet::Wallet(SignContainer *container, const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr)
   , signContainer_(container), logger_(logger)
{}

Wallet::~Wallet()
{
   UtxoReservation::delAdapter(utxoAdapter_);
}

void Wallet::synchronize(const std::function<void()> &cbDone)
{
   const auto &cbProcess = [this, cbDone] (bs::sync::WalletData data) {
      netType_ = data.netType;
      for (const auto &addr : data.addresses) {
         addAddress(addr.address, addr.index, addr.address.getType(), false);
         setAddressComment(addr.address, addr.comment, false);
      }
      for (const auto &txComment : data.txComments) {
         setTransactionComment(txComment.txHash, txComment.comment, false);
      }
      if (cbDone) {
         cbDone();
      }
   };
   signContainer_->syncWallet(walletId(), cbProcess);
}

std::string Wallet::getAddressComment(const bs::Address &address) const
{
   const auto &itComment = addrComments_.find(address);
   if (itComment != addrComments_.end()) {
      return itComment->second;
   }
   return {};
}

bool Wallet::setAddressComment(const bs::Address &address, const std::string &comment, bool sync)
{
   if (address.isNull() || comment.empty()) {
      return false;
   }
   addrComments_[address] = comment;
   if (sync && signContainer_) {
      signContainer_->syncAddressComment(walletId(), address, comment);
   }
   emit addressAdded();
   return true;
}

std::string Wallet::getTransactionComment(const BinaryData &txHash)
{
   const auto &itComment = txComments_.find(txHash);
   if (itComment != txComments_.end()) {
      return itComment->second;
   }
   return {};
}

bool Wallet::setTransactionComment(const BinaryData &txOrHash, const std::string &comment, bool sync)
{
   if (txOrHash.isNull() || comment.empty()) {
      return false;
   }
   BinaryData txHash;
   if (txOrHash.getSize() == 32) {
      txHash = txOrHash;
   } else {   // raw transaction then
      Tx tx(txOrHash);
      if (!tx.isInitialized()) {
         return false;
      }
      txHash = tx.getThisHash();
   }
   txComments_[txHash] = comment;
   if (sync && signContainer_) {
      signContainer_->syncTxComment(walletId(), txHash, comment);
   }
   return true;   //stub
}

bool Wallet::isBalanceAvailable() const
{
   return (armory_ != nullptr) && (armory_->state() == ArmoryConnection::State::Ready) && (btcWallet_ != nullptr);
}

BTCNumericTypes::balance_type Wallet::getSpendableBalance() const
{
   if (!isBalanceAvailable()) {
      return std::numeric_limits<double>::infinity();
   }
   return spendableBalance_;
}

BTCNumericTypes::balance_type Wallet::getUnconfirmedBalance() const
{
   if (!isBalanceAvailable()) {
      return 0;
   }
   return unconfirmedBalance_;
}

BTCNumericTypes::balance_type Wallet::getTotalBalance() const
{
   if (!isBalanceAvailable()) {
      return std::numeric_limits<double>::infinity();
   }
   return totalBalance_;
}

bool Wallet::getAddrBalance(const bs::Address &addr, std::function<void(std::vector<uint64_t>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   static const std::vector<uint64_t> defVal = { 0, 0, 0 };

   if (updateAddrBalance_) {
      const auto &cbAddrBalance = [this]
         (ReturnMessage<std::map<BinaryData, std::vector<uint64_t>>> balanceMap) {
         try {
            const auto bm = balanceMap.get();
            updateMap<std::map<BinaryData, std::vector<uint64_t>>>(bm, addressBalanceMap_);
            updateAddrBalance_ = false;
         }
         catch(std::exception& e) {
            if(logger_ != nullptr) {
               logger_->error("[getAddrBalance (cbAddrBalance)] Return data " \
                              "error - {}", e.what());
            }
         }

         invokeCb<std::vector<uint64_t>>(addressBalanceMap_, cbBal_, defVal);
      };

      cbBal_[addr].push_back(cb);
      if (cbBal_.size() == 1) {
         btcWallet_->getAddrBalancesFromDB(cbAddrBalance);
      }
   }
   else {
      const auto itBal = addressBalanceMap_.find(addr.id());
      if (itBal == addressBalanceMap_.end()) {
         cb(defVal);
         return true;
      }
      cb(itBal->second);
   }
   return true;
}

bool Wallet::getAddrTxN(const bs::Address &addr, std::function<void(uint32_t)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   if (updateAddrTxN_) {
      const auto &cbTxN = [this, addr]
                        (ReturnMessage<std::map<BinaryData, uint32_t>> txnMap) {
         try {
            const auto inTxnMap = txnMap.get();
            updateMap<std::map<BinaryData, uint32_t>>(inTxnMap, addressTxNMap_);
            updateAddrTxN_ = false;
         }
         catch (const std::exception &e) {
            if (logger_ != nullptr) {
               logger_->error("[bs::sync::Wallet::getAddrTxN] Return data error - {} ", \
                  "- Address {}", e.what(), addr.display());
            }
         }

         invokeCb<uint32_t>(addressTxNMap_, cbTxN_, 0);
      };

      cbTxN_[addr].push_back(cb);
      if (cbTxN_.size() == 1) {
         btcWallet_->getAddrTxnCountsFromDB(cbTxN);
      }
   }
   else {
      const auto itTxN = addressTxNMap_.find(addr.id());
      if (itTxN == addressTxNMap_.end()) {
         cb(0);
         return true;
      }
      cb(itTxN->second);
   }
   return true;
}

bool Wallet::getActiveAddressCount(const std::function<void(size_t)> &cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   if (addressTxNMap_.empty() || updateAddrTxN_) {
      const auto &cbTxN = [this, cb] (ReturnMessage<std::map<BinaryData, uint32_t>> txnMap) {
         try {
            auto inTxnMap = txnMap.get();
            updateMap<std::map<BinaryData, uint32_t>>(inTxnMap, addressTxNMap_);
            updateAddrTxN_ = false;
            cb(addressTxNMap_.size());
         } catch (std::exception& e) {
            if (logger_ != nullptr) {
               logger_->error("[bs::sync::Wallet::GetActiveAddressCount] Return data error - {} ", e.what());
            }
         }
      };
      btcWallet_->getAddrTxnCountsFromDB(cbTxN);
   } else {
      cb(addressTxNMap_.size());
   }
   return true;
}

bool Wallet::getSpendableTxOutList(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , std::function<void(std::vector<UTXO>)> cb, QObject *obj, uint64_t val)
{
   if (!isBalanceAvailable()) {
      return false;
   }

   if (obj) {
      auto &callbacks = spendableCallbacks_[btcWallet->walletID()];
      callbacks.push_back({ obj, cb });
      if (callbacks.size() > 1) {
         return true;
      }
   }

   const auto &cbTxOutList = [this, obj, cb, val, btcWallet]
                             (ReturnMessage<std::vector<UTXO>> txOutList) {
      try {
         // Before invoking the callbacks, process the UTXOs for the purposes of
         // handling internal/external addresses (UTXO filtering, balance
         // adjusting, etc.).
         auto txOutListObj = txOutList.get();
         const auto &cbProcess = [this, obj, cb, val, btcWallet, txOutListObj] {
            std::vector<UTXO> txOutListCopy = txOutListObj;
            if (utxoAdapter_) {
               utxoAdapter_->filter(txOutListCopy);
            }
            if (val != UINT64_MAX) {
               uint64_t sum = 0;
               int cutOffIdx = -1;
               for (size_t i = 0; i < txOutListCopy.size(); i++) {
                  const auto &utxo = txOutListCopy[i];
                  sum += utxo.getValue();
                  if (sum >= val) {
                     cutOffIdx = (int)i;
                     break;
                  }
               }
               if (cutOffIdx >= 0) {
                  txOutListCopy.resize(cutOffIdx + 1);
               }
            }
            if (obj == nullptr) {
               if (cb) {
                  cb(txOutListCopy);
               }
            }
            else {
               QMetaObject::invokeMethod(this, [this, btcWallet, txOutListCopy] {
                  auto &callbacks = spendableCallbacks_[btcWallet->walletID()];
                  for (const auto &cbPairs : callbacks) {
                     if (cbPairs.first) {
                        cbPairs.second(txOutListCopy);
                     }
                  }
                  spendableCallbacks_.erase(btcWallet->walletID());
               });
            }
         };

         cbProcess();
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::sync::Wallet::getSpendableTxOutList] Return data " \
               "error {} - value {}", e.what(), val);
         }
      }
   };
   btcWallet->getSpendableTxOutListForValue(val, cbTxOutList);
   return true;
}

bool Wallet::getSpendableTxOutList(std::function<void(std::vector<UTXO>)> cb
   , QObject *obj, uint64_t val)
{
   return getSpendableTxOutList(btcWallet_, cb, obj, val);
}

bool Wallet::getUTXOsToSpend(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , uint64_t val, std::function<void(std::vector<UTXO>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   const auto &cbProcess = [this, val, cb]
                           (ReturnMessage<std::vector<UTXO>> utxos)-> void {
      try {
         auto utxosObj = utxos.get();
         if (utxoAdapter_) {
            utxoAdapter_->filter(utxosObj);
         }
         std::sort(utxosObj.begin(), utxosObj.end(), [](const UTXO &a, const UTXO &b) {
            return (a.getValue() < b.getValue());
         });

         int index = (int)utxosObj.size() - 1;
         while (index >= 0) {
            if (utxosObj[index].getValue() < val) {
               index++;
               break;
            }
            index--;
         }
         if ((index >= 0) && ((size_t)index < utxosObj.size())) {
            cb({ utxosObj[index] });
            return;
         }
         else if (index < 0) {
            cb({ utxosObj.front() });
            return;
         }

         std::vector<UTXO> result;
         uint64_t sum = 0;
         index = (int)utxosObj.size() - 1;
         while ((index >= 0) && (sum < val)) {  //TODO: needs to be optimized to fill the val more precisely
            result.push_back(utxosObj[index]);
            sum += utxosObj[index].getValue();
            index--;
         }

         if (sum < val) {
            cb({});
         }
         else {
            cb(result);
         }
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::sync::Wallet::getUTXOsToSpend] Return data error " \
               "- {} - value {}", e.what(), val);
         }
      }
   };
   btcWallet->getSpendableTxOutListForValue(val, cbProcess);
   return true;
}

bool Wallet::getUTXOsToSpend(uint64_t val, std::function<void(std::vector<UTXO>)> cb) const
{
   return getUTXOsToSpend(btcWallet_, val, cb);
}

bool Wallet::getSpendableZCList(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , std::function<void(std::vector<UTXO>)> cb, QObject *obj)
{
   if (!btcWallet) {
      return false;
   }

   auto &cbList = zcListCallbacks_[btcWallet->walletID()];
   cbList.push_back({ obj, cb });
   if (cbList.size() > 1) {
      return true;
   }
   const auto &cbZCList = [this, btcWallet](ReturnMessage<std::vector<UTXO>> utxos)-> void {
      try {
         auto inUTXOs = utxos.get();
         // Before invoking the callbacks, process the UTXOs for the purposes of
         // handling internal/external addresses (UTXO filtering, balance
         // adjusting, etc.).
         const auto &cbProcess = [this, btcWallet, inUTXOs] {
            QMetaObject::invokeMethod(this, [this, btcWallet, inUTXOs] {
               const auto &itCb = zcListCallbacks_.find(btcWallet->walletID());
               if (itCb == zcListCallbacks_.end()) {
                  logger_->error("[sync::Wallet::getSpendableZCList] failed to find callback for id {}"
                     , btcWallet->walletID());
                  return;
               }
               for (const auto &cb : itCb->second) {
                  if (cb.first) {
                     cb.second(inUTXOs);
                  }
               }
               zcListCallbacks_.erase(itCb);
            });
         };
         cbProcess();
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::sync::Wallet::getSpendableZCList] Return data error " \
               "- {}", e.what());
         }
      }
   };
   btcWallet->getSpendableZCList(cbZCList);
   return true;
}

bool Wallet::getSpendableZCList(std::function<void(std::vector<UTXO>)> cb
   , QObject *obj)
{
   return getSpendableZCList(btcWallet_, cb, obj);
}

bool Wallet::getRBFTxOutList(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , std::function<void(std::vector<UTXO>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }

   // The callback we passed in needs data from Armory. Write a simple callback
   // that takes Armory's data and uses it in the callback.
   const auto &cbArmory = [this, cb](ReturnMessage<std::vector<UTXO>> utxos)->void {
      try {
         auto inUTXOs = utxos.get();
         cb(std::move(inUTXOs));
      }
      catch(std::exception& e) {
         if(logger_ != nullptr) {
            logger_->error("[bs::sync::Wallet::getRBFTxOutList] Return data error - " \
               "{}", e.what());
         }
      }
   };

   btcWallet->getRBFTxOutList(cbArmory);
   return true;
}

bool Wallet::getRBFTxOutList(std::function<void(std::vector<UTXO>)> cb) const
{
   return getRBFTxOutList(btcWallet_, cb);
}

// Public frontend for updating a wallet's balances. Required in part because
// Armory doesn't declare TXs safe until 6 confs have occurred.
void Wallet::updateBalances(const std::function<void(std::vector<uint64_t>)> &cb)
{
   if (!isBalanceAvailable()) {
      return;
   }
   const auto &cbBalances = [this, cb]
                    (ReturnMessage<std::vector<uint64_t>> balanceVector)->void {
      try {
         auto bv = balanceVector.get();
         if (bv.size() < 4) {
            return;
         }
         const auto totalBalance =
            static_cast<BTCNumericTypes::balance_type>(bv[0]) / BTCNumericTypes::BalanceDivider;
         const auto spendableBalance =
            static_cast<BTCNumericTypes::balance_type>(bv[1]) / BTCNumericTypes::BalanceDivider;
         const auto unconfirmedBalance =
            static_cast<BTCNumericTypes::balance_type>(bv[2]) / BTCNumericTypes::BalanceDivider;
         const auto count = bv[3];

         if ((addrCount_ != count) || (totalBalance_ != totalBalance) || (spendableBalance_ != spendableBalance)
            || (unconfirmedBalance_ != unconfirmedBalance)) {
            {
               QMutexLocker lock(&addrMapsMtx_);
               updateAddrBalance_ = true;
               updateAddrTxN_ = true;
               addrCount_ = count;
            }
            totalBalance_ = totalBalance;
            spendableBalance_ = spendableBalance;
            unconfirmedBalance_ = unconfirmedBalance;

            emit balanceChanged(walletId(), bv);
         }
         emit balanceUpdated(walletId(), bv);

         if (cb) {
            cb(bv);
         }
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::sync::Wallet::UpdateBalances] Return data error " \
               "- {}", e.what());
         }
      }
   };
   btcWallet_->getBalancesAndCount(armory_->topBlock(), cbBalances);
}

bool Wallet::getHistoryPage(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , uint32_t id, std::function<void(const Wallet *wallet
   , std::vector<ClientClasses::LedgerEntry>)> clientCb, bool onlyNew) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   const auto &cb = [this, id, onlyNew, clientCb]
                    (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
      try {
         auto le = entries.get();
         if (!onlyNew) {
            clientCb(this, le);
         }
         else {
            const auto &histPage = historyCache_.find(id);
            if (histPage == historyCache_.end()) {
               clientCb(this, le);
            }
            else if (histPage->second.size() == le.size()) {
               clientCb(this, {});
            }
            else {
               std::vector<ClientClasses::LedgerEntry> diff;
               struct comparator {
                  bool operator() (const ClientClasses::LedgerEntry &a, const ClientClasses::LedgerEntry &b) const {
                     return (a.getTxHash() < b.getTxHash());
                  }
               };
               std::set<ClientClasses::LedgerEntry, comparator> diffSet;
               diffSet.insert(le.begin(), le.end());
               for (const auto &entry : histPage->second) {
                  diffSet.erase(entry);
               }
               for (const auto &diffEntry : diffSet) {
                  diff.emplace_back(diffEntry);
               }
               clientCb(this, diff);
            }
         }
         historyCache_[id] = le;
      }
      catch (const std::exception& e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::sync::Wallet::getHistoryPage] Return data " \
               "error - {} - ID {}", e.what(), id);
         }
      }
   };
   btcWallet->getHistoryPage(id, cb);
   return true;
}

bool Wallet::getHistoryPage(uint32_t id, std::function<void(const Wallet *wallet
   , std::vector<ClientClasses::LedgerEntry>)> clientCb, bool onlyNew) const
{
   return getHistoryPage(btcWallet_, id, clientCb, onlyNew);
}

bs::Address Wallet::getRandomChangeAddress(AddressEntryType aet, const CbAddress &cb)
{
   if (getUsedAddressCount() < 3) {
      return getNewChangeAddress(aet, cb);
   }
   const auto &addresses = getUsedAddressList();
   return addresses[qrand() % addresses.size()];
}

QString Wallet::displayTxValue(int64_t val) const
{
   return QLocale().toString(val / BTCNumericTypes::BalanceDivider, 'f', BTCNumericTypes::default_precision);
}

void Wallet::setArmory(const std::shared_ptr<ArmoryObject> &armory)
{
   if (!armory_ && (armory != nullptr)) {
      armory_ = armory;
   }

   if (!utxoAdapter_) {
      utxoAdapter_ = std::make_shared<UtxoFilterAdapter>(walletId());
      if (!UtxoReservation::addAdapter(utxoAdapter_)) {
         utxoAdapter_ = nullptr;
      }
   }
}

std::vector<std::string> Wallet::registerWallet(const std::shared_ptr<ArmoryObject> &armory, bool asNew)
{
   setArmory(armory);

   if (armory_) {
      const auto &cbRegister = [this](const std::string &) {
         logger_->debug("Wallet ready: {}", walletId());
         emit walletReady(QString::fromStdString(walletId()));
      };
      const auto regId = armory_->registerWallet(btcWallet_, walletId(), getAddrHashes(), cbRegister, asNew);
      logger_->debug("register wallet {}, {} addresses = {}", walletId(), getAddrHashes().size(), regId);
      return { regId };
   }
   return {};
}

void Wallet::unregisterWallet()
{
   heartbeatRunning_ = false;
   btcWallet_.reset();
   {
      QMutexLocker lock(&addrMapsMtx_);
      cbBal_.clear();
      cbTxN_.clear();
   }
   spendableCallbacks_.clear();
   zcListCallbacks_.clear();
   historyCache_.clear();

   if (armory_) {
      armory_->registerWallet(btcWallet_, walletId(), {}, [](const std::string &){}, false);
   }
   btcWallet_.reset();
}

void Wallet::firstInit(bool force)
{
   updateBalances();
}

bs::core::wallet::TXSignRequest wallet::createTXRequest(const std::string &walletId
   , const std::vector<UTXO> &inputs
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients
   , const std::function<bs::Address(std::string &)> &cbChangeAddr
   , const uint64_t fee, bool isRBF, const uint64_t &origFee)
{
   bs::core::wallet::TXSignRequest request;
   request.walletId = walletId;

   uint64_t inputAmount = 0;
   uint64_t spendAmount = 0;

   if (inputs.empty()) {
      throw std::logic_error("no UTXOs");
   }

   for (const auto& utxo : inputs) {
      inputAmount += utxo.getValue();
   }
   request.inputs = inputs;

   for (const auto& recipient : recipients) {
      if (recipient == nullptr) {
         throw std::logic_error("invalid recipient");
      }
      spendAmount += recipient->getValue();
   }
   if (inputAmount < spendAmount + fee) {
      throw std::logic_error("input amount " + std::to_string(inputAmount) + " is less than spend + fee (" + std::to_string(spendAmount + fee) + ")");
   }

   request.recipients = recipients;
   request.RBF = isRBF;

   if (isRBF && (fee < wallet::kMinRelayFee)) {
      request.fee = wallet::kMinRelayFee;
   } else {
      request.fee = fee;
   }

   const uint64_t changeAmount = inputAmount - (spendAmount + fee);
   if (changeAmount) {
      if (cbChangeAddr) {
         const auto changeAddress = cbChangeAddr(request.change.index);
         if (!changeAddress.isNull()) {
            request.change.value = changeAmount;
            request.change.address = changeAddress;
         }
         else if (changeAmount >= fee) {
            throw std::runtime_error("failed to get change address");
         }
      }
      else {
         throw std::logic_error("can't get change address for " + std::to_string(changeAmount));
      }
   }

   return request;
}

bs::core::wallet::TXSignRequest Wallet::createTXRequest(const std::vector<UTXO> &inputs
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients, const uint64_t fee
   , bool isRBF, bs::Address changeAddress, const uint64_t& origFee)
{
   const auto &cbNewChangeAddr = [this](std::string &index) -> bs::Address {
      const auto result = getNewChangeAddress();
      setAddressComment(result, wallet::Comment::toString(wallet::Comment::ChangeAddress));
      index = getAddressIndex(result);
      return result;
   };
   const auto &cbChangeAddr = [changeAddress, cbNewChangeAddr](std::string &index) {
      if (changeAddress.isNull()) {
         return cbNewChangeAddr(index);
      }
      return changeAddress;
   };
   return wallet::createTXRequest(walletId(), inputs, recipients, cbChangeAddr
      , fee, isRBF, origFee);
}

bs::core::wallet::TXSignRequest Wallet::createPartialTXRequest(uint64_t spendVal
   , const std::vector<UTXO> &inputs, bs::Address changeAddress
   , float feePerByte
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients
   , const BinaryData prevPart)
{
   uint64_t inputAmount = 0;
   uint64_t fee = 0;
   auto utxos = inputs;
   if (utxos.empty()) {
      throw std::invalid_argument("No usable UTXOs");
   }

   if (feePerByte > 0) {
      unsigned int idMap = 0;
      std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipMap;
      for (const auto &recip : recipients) {
         if (recip->getValue()) {
            recipMap.emplace(idMap++, recip);
         }
      }

      PaymentStruct payment(recipMap, 0, feePerByte, ADJUST_FEE);
      for (auto &utxo : utxos) {
         const auto scrAddr = bs::Address(utxo.getRecipientScrAddr());
         utxo.txinRedeemSizeBytes_ = (unsigned int)scrAddr.getInputSize();
         inputAmount += utxo.getValue();
      }

      if (!inputAmount) {
         throw std::invalid_argument("Couldn't find address entries for UTXOs");
      }

      const auto coinSelection = std::make_shared<CoinSelection>([utxos](uint64_t) { return utxos; }
         , std::vector<AddressBookEntry>{}, getSpendableBalance() * BTCNumericTypes::BalanceDivider
         , armory_ ? armory_->topBlock() : UINT32_MAX);

      try {
         const auto selection = coinSelection->getUtxoSelectionForRecipients(payment, utxos);
         fee = selection.fee_;
         utxos = selection.utxoVec_;
         inputAmount = selection.value_;
      }
      catch (...) {}
   }
   else {
      size_t nbUtxos = 0;
      for (auto &utxo : utxos) {
         inputAmount += utxo.getValue();
         nbUtxos++;
         if (inputAmount >= (spendVal + fee)) {
            break;
         }
      }
      if (nbUtxos < utxos.size()) {
         utxos.erase(utxos.begin() + nbUtxos, utxos.end());
      }
   }

   if (utxos.empty()) {
      throw std::logic_error("No UTXOs");
   }

   bs::core::wallet::TXSignRequest request;
   request.walletId = walletId();
//   request.wallet = this;
   request.populateUTXOs = true;
   Signer signer;
   if (!prevPart.isNull()) {
      signer.deserializeState(prevPart);
      if (feePerByte > 0) {
         bs::CheckRecipSigner chkSigner(prevPart);
         fee += chkSigner.estimateFee(feePerByte);
      }
   }
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);
   request.fee = fee;

   inputAmount = 0;
   for (const auto& utxo : utxos) {
      signer.addSpender(std::make_shared<ScriptSpender>(utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      request.inputs.push_back(utxo);
      inputAmount += utxo.getValue();
      if (inputAmount >= (spendVal + fee)) {
         break;
      }
   }
   if (!inputAmount) {
      throw std::logic_error("No inputs detected");
   }

   if (!recipients.empty()) {
      uint64_t spendAmount = 0;
      for (const auto& recipient : recipients) {
         if (recipient == nullptr) {
            throw std::logic_error("Invalid recipient");
         }
         spendAmount += recipient->getValue();
         signer.addRecipient(recipient);
      }
      if (spendAmount != spendVal) {
         throw std::invalid_argument("Recipient[s] amount != spend value");
      }
   }
   request.recipients = recipients;

   if (inputAmount > (spendVal + fee)) {
      const uint64_t changeVal = inputAmount - (spendVal + fee);
      if (changeAddress.isNull()) {
         throw std::invalid_argument("Change address required, but missing");
      }
      signer.addRecipient(changeAddress.getRecipient(changeVal));
      request.change.value = changeVal;
      request.change.address = changeAddress;
      request.change.index = getAddressIndex(changeAddress);
   }

   request.prevStates.emplace_back(signer.serializeState());
   return request;
}

bool Wallet::getLedgerDelegateForAddress(const bs::Address &addr
   , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &cb
   , QObject *context)
{
   if (armory_) {
      return armory_->getLedgerDelegateForAddress(walletId(), addr, cb, context);
   }
   return false;
}

int Wallet::addAddress(const bs::Address &addr, const std::string &index, AddressEntryType aet, bool sync)
{
   if (!addr.isNull()) {
      usedAddresses_.push_back(addr);
   }
   if (sync && signContainer_) {
      std::string idxCopy = index;
      if (idxCopy.empty() && !addr.isNull()) {
         aet = addr.getType();
         idxCopy = getAddressIndex(addr);
         if (idxCopy.empty()) {
            idxCopy = addr.display();
         }
      }
      signContainer_->syncNewAddress(walletId(), idxCopy, aet, [](const bs::Address &) {});
   }
   return (usedAddresses_.size() - 1);
}

void Wallet::newAddresses(const std::vector<std::pair<std::string, AddressEntryType>> &inData
   , const CbAddresses &cb, bool persistent)
{
   if (signContainer_) {
      signContainer_->syncNewAddresses(walletId(), inData, cb);
   }
   else {
      if (logger_) {
         logger_->warn("[{}] no signer set", __func__);
      }
   }
}
