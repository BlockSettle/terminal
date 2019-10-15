#include "SyncWallet.h"
#include <QLocale>
#include <bech32/ref/c++/segwit_addr.h>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "CoinSelection.h"
#include "WalletSignerContainer.h"

using namespace bs::sync;

Wallet::Wallet(WalletSignerContainer *container, const std::shared_ptr<spdlog::logger> &logger)
   : signContainer_(container), logger_(logger)
{
   balanceData_ = std::make_shared<BalanceData>();
}

Wallet::~Wallet()
{
   {
      std::unique_lock<std::mutex> lock(balThrMutex_);
      balThreadRunning_ = false;
      balThrCV_.notify_one();
   }
   {
      std::unique_lock<std::mutex> lock(balanceData_->cbMutex);
      balanceData_->cbBalances.clear();
      balanceData_->cbTxNs.clear();
   }
   UtxoReservation::delAdapter(utxoAdapter_);
}

const std::string& Wallet::walletIdInt(void) const
{
   /***
   Overload this if your wallet class supports internal chains.
   A wallet object without an internal chain should throw a
   runtime error.
   ***/

   throw std::runtime_error("no internal chain");
}

void Wallet::synchronize(const std::function<void()> &cbDone)
{
   const auto &cbProcess = [this, cbDone] (bs::sync::WalletData data)
   {
      usedAddresses_.clear();
      for (const auto &addr : data.addresses) {
         addAddress(addr.address, addr.index, false);
         setAddressComment(addr.address, addr.comment, false);
      }

      for (const auto &txComment : data.txComments)
         setTransactionComment(txComment.txHash, txComment.comment, false);

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
   if (wct_ && sync) {
      wct_->addressAdded(walletId());
   }
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
   return
      (armory_ != nullptr) &&
      (armory_->state() == ArmoryState::Ready) &&
      isRegistered();
}

BTCNumericTypes::balance_type Wallet::getSpendableBalance() const
{
   if (!isBalanceAvailable()) {
      return std::numeric_limits<double>::infinity();
   }
   return balanceData_->spendableBalance;
}

BTCNumericTypes::balance_type Wallet::getUnconfirmedBalance() const
{
   if (!isBalanceAvailable()) {
      return 0;
   }
   return balanceData_->unconfirmedBalance;
}

BTCNumericTypes::balance_type Wallet::getTotalBalance() const
{
   if (!isBalanceAvailable()) {
      return std::numeric_limits<double>::infinity();
   }
   return balanceData_->totalBalance;
}

std::vector<uint64_t> Wallet::getAddrBalance(const bs::Address &addr) const
{
   if (!isBalanceAvailable()) {
      throw std::runtime_error("uninitialized db connection");
   }
   std::unique_lock<std::mutex> lock(balanceData_->addrMapsMtx);

   auto iter = balanceData_->addressBalanceMap.find(addr.prefixed());
   if (iter == balanceData_->addressBalanceMap.end()) {
      return {};
   }

   return iter->second;
}

uint64_t Wallet::getAddrTxN(const bs::Address &addr) const
{
   if (!isBalanceAvailable()) {
      throw std::runtime_error("uninitialized db connection");
   }
   std::unique_lock<std::mutex> lock(balanceData_->addrMapsMtx);

   auto iter = balanceData_->addressTxNMap.find(addr.prefixed());
   if (iter == balanceData_->addressTxNMap.end()) {
      return 0;
   }

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
////
//// Combined DB fetch methods
////
////////////////////////////////////////////////////////////////////////////////

bool Wallet::updateBalances(const std::function<void(void)> &cb)
{  /***
   The callback is only used to signify request completion, use the
   get methods to grab the individual balances
   ***/
   if (!armory_) {
      return false;
   }

   size_t cbSize = 0;
   {
      std::unique_lock<std::mutex> lock(balanceData_->cbMutex);
      cbSize = balanceData_->cbBalances.size();
      balanceData_->cbBalances.push_back(cb);
   }
   if (cbSize == 0) {
      std::vector<std::string> walletIDs;
      walletIDs.push_back(walletId());
      try {
         walletIDs.push_back(walletIdInt());
      } catch (std::exception&) {}

      const auto onCombinedBalances = [balanceData = balanceData_]
         (const std::map<std::string, CombinedBalances> &balanceMap)
      {
         bool ourUpdate = false;
         BTCNumericTypes::balance_type total = 0, spendable = 0, unconfirmed = 0;
         uint64_t addrCount = 0;
         for (const auto &wltBal : balanceMap) {
            ourUpdate = true;

            total += static_cast<BTCNumericTypes::balance_type>(
               wltBal.second.walletBalanceAndCount_[0]) / BTCNumericTypes::BalanceDivider;
            /*      spendable += static_cast<BTCNumericTypes::balance_type>(
                     wltBal.second.walletBalanceAndCount_[1]) / BTCNumericTypes::BalanceDivider;*/
            unconfirmed += static_cast<BTCNumericTypes::balance_type>(
               wltBal.second.walletBalanceAndCount_[2]) / BTCNumericTypes::BalanceDivider;

            //wallet txn count
            addrCount += wltBal.second.walletBalanceAndCount_[3];

            { //address balances
               std::unique_lock<std::mutex> lock(balanceData->addrMapsMtx);
               updateMap<std::map<BinaryData, std::vector<uint64_t>>>(
                  wltBal.second.addressBalances_, balanceData->addressBalanceMap);
            }
         }
         spendable = total - unconfirmed;

         if (ourUpdate) {
            balanceData->totalBalance = total;
            balanceData->spendableBalance = spendable;
            balanceData->unconfirmedBalance = unconfirmed;
            balanceData->addrCount = addrCount;

            std::vector<std::function<void(void)>> cbCopy;
            {
               std::unique_lock<std::mutex> lock(balanceData->cbMutex);
               cbCopy.swap(balanceData->cbBalances);
            }
            for (const auto &cb : cbCopy) {
               if (cb) {
                  cb();
               }
            }
         }
      };
      return armory_->getCombinedBalances(walletIDs, onCombinedBalances);
   } else {          // if the callbacks queue is not empty, don't call
      return true;   // armory's RPC - just add the callback and return
   }
}

bool Wallet::getSpendableTxOutList(const ArmoryConnection::UTXOsCb &cb, uint64_t val)
{
   //combined utxo fetch method

   if (!isBalanceAvailable()) {
      return false;
   }

   const auto &cbTxOutList = [this, val, cb]
      (const std::vector<UTXO> &txOutList) {
      std::vector<UTXO> txOutListCopy = txOutList;
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

      cb(txOutListCopy);
   };

   std::vector<std::string> walletIDs;
   walletIDs.push_back(walletId());
   try {
      walletIDs.push_back(walletIdInt());
   }
   catch(std::exception&)
   {}

   return armory_->getSpendableTxOutListForValue(walletIDs, val, cbTxOutList);
}

bool Wallet::getSpendableZCList(const ArmoryConnection::UTXOsCb &cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }

   std::vector<std::string> walletIDs;
   walletIDs.push_back(walletId());
   try {
      walletIDs.push_back(walletIdInt());
   }
   catch (std::exception&)
   {}

   armory_->getSpendableZCoutputs(walletIDs, cb);
   return true;
}

bool Wallet::getRBFTxOutList(const ArmoryConnection::UTXOsCb &cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }

   std::vector<std::string> walletIDs;
   walletIDs.push_back(walletId());
   try {
      walletIDs.push_back(walletIdInt());
   }
   catch (std::exception&)
   {}

   armory_->getRBFoutputs(walletIDs, cb);
   return true;
}

bool Wallet::getSpendableTxOutList(const std::vector<std::shared_ptr<Wallet>> &wallets, ArmoryConnection::UTXOsCb cb)
{
   if (wallets.empty()) {
      cb({});
      return true;
   }
   struct Result
   {
      std::map<int, std::vector<UTXO>> utxosMap;
      ArmoryConnection::UTXOsCb cb;
   };
   auto result = std::make_shared<Result>();
   result->cb = std::move(cb);

   for (size_t i = 0; i < wallets.size(); ++i) {
      const auto &wallet = wallets[i];
      auto cbWrap = [i, result, size = wallets.size()](std::vector<UTXO> utxos) {
         result->utxosMap.emplace(i, std::move(utxos));
         if (result->utxosMap.size() != size) {
            return;
         }

         size_t total = 0;
         for (auto &item : result->utxosMap) {
            total += item.second.size();
         }
         std::vector<UTXO> utxosAll;
         utxosAll.reserve(total);

         for (auto &item : result->utxosMap) {
            utxosAll.insert(utxosAll.end(), std::make_move_iterator(item.second.begin())
               , std::make_move_iterator(item.second.end()));
         }
         result->cb(utxosAll);
      };
      // If request for some wallet failed resulted callback won't be called.
      bool status = wallet->getSpendableTxOutList(cbWrap, UINT64_MAX);
      if (!status) {
         return false;
      }
   }
   return true;
}

void Wallet::setWCT(WalletCallbackTarget *wct)
{
   wct_ = wct;
}

bool Wallet::getAddressTxnCounts(const std::function<void(void)> &cb)
{  /***
   Same as updateBalances, this methods grabs the addr txn count
   for all addresses in wallet (inner chain included) and caches
   them locally.

   Use getAddressTxnCount to get a specific count for a given
   address from the cache.
   ***/
   if (!armory_) {
      return false;
   }
   size_t cbSize = 0;
   {
      std::unique_lock<std::mutex> lock(balanceData_->cbMutex);
      cbSize = balanceData_->cbTxNs.size();
      balanceData_->cbTxNs.push_back(cb);
   }
   if (cbSize == 0) {
      std::vector<std::string> walletIDs;
      walletIDs.push_back(walletId());
      try {
         walletIDs.push_back(walletIdInt());
      } catch (std::exception&) {}

      const auto &cbTxNs = [balanceData = balanceData_]
         (const std::map<std::string, CombinedCounts> &countMap)
      {
         for (const auto &count : countMap) {
            std::unique_lock<std::mutex> lock(balanceData->addrMapsMtx);
            updateMap<std::map<BinaryData, uint64_t>>(
               count.second.addressTxnCounts_, balanceData->addressTxNMap);
         }

         auto cbTxNsCopy = std::make_shared<std::vector<std::function<void(void)>>>();
         {
            std::unique_lock<std::mutex> lock(balanceData->cbMutex);
            cbTxNsCopy->swap(balanceData->cbTxNs);
         }
         for (const auto &cb : *cbTxNsCopy) {
            if (cb) {
               cb();
            }
         }
      };
      return armory_->getCombinedTxNs(walletIDs, cbTxNs);
   }
   else {
      return true;
   }
}

////////////////////////////////////////////////////////////////////////////////

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

QString Wallet::displayTxValue(int64_t val) const
{
   return QLocale().toString(val / BTCNumericTypes::BalanceDivider, 'f', BTCNumericTypes::default_precision);
}

void Wallet::setArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   if (!armory_ && (armory != nullptr)) {
      armory_ = armory;

      /*
      Do not set callback target if it is already initialized. This
      allows for unit tests to set a custom ACT.
      */
      if(act_ == nullptr) {
         act_ = make_unique<WalletACT>(this);
         act_->init(armory_.get());
      }
   }

   if (!utxoAdapter_) {
      utxoAdapter_ = std::make_shared<UtxoFilterAdapter>(walletId());
      if (!UtxoReservation::addAdapter(utxoAdapter_)) {
         utxoAdapter_ = nullptr;
      }
   }
}

void Wallet::onZeroConfReceived(const std::vector<bs::TXEntry> &entries)
{
   init(true);

   const auto &cbTX = [this, balanceData = balanceData_](const Tx &tx) {
      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         const TxIn in = tx.getTxInCopy(i);
         const OutPoint op = in.getOutPoint();

         const auto &cbPrevTX = [this, balanceData, idx=op.getTxOutIndex()](const Tx &prevTx) {
            if (!prevTx.isInitialized()) {
               return;
            }
            const TxOut prevOut = prevTx.getTxOutCopy(idx);
            const auto addr = bs::Address::fromTxOut(prevOut);
            if (!containsAddress(addr)) {
               return;
            }
            bool updated = false;
            {
               std::unique_lock<std::mutex> lock(balanceData->addrMapsMtx);
               const auto &itTxn = balanceData->addressTxNMap.find(addr.id());
               if (itTxn != balanceData->addressTxNMap.end()) {
                  itTxn->second++;
                  updated = true;
               }
            }
            if (updated && wct_) {
               wct_->balanceUpdated(walletId());
            }
         };
         armory_->getTxByHash(op.getTxHash(), cbPrevTX);
      }
   };
   for (const auto &entry : entries) {
      armory_->getTxByHash(entry.txHash, cbTX);
   }
   updateBalances([this] {    // TxNs are not updated for ZCs
      trackChainAddressUse([this](bs::sync::SyncState st) {
         logger_->debug("{}: new live address found: {}", walletId(), (int)st);
         if (st == bs::sync::SyncState::Success) {
            synchronize([this] {
               logger_->debug("[Wallet::onZeroConfReceived] synchronized after addresses are tracked");
               if (wct_) {
                  wct_->addressAdded(walletId());
               }
            });
         }
      });
   });
}

void Wallet::onNewBlock(unsigned int depth)
{
   init(true);
}

void Wallet::onBalanceAvailable(const std::function<void()> &cb) const
{
   if (isBalanceAvailable()) {
      if (cb) {
         cb();
      }
      return;
   }

   const auto thrBalAvail = [this, cb] {
      while (balThreadRunning_) {
         std::unique_lock<std::mutex> lock(balThrMutex_);
         balThrCV_.wait_for(lock, std::chrono::milliseconds{ 100 });
         if (!balThreadRunning_) {
            return;
         }
         if (isBalanceAvailable()) {
            for (const auto &cb : cbBalThread_) {
               if (cb) {
                  cb();
               }
            }
            cbBalThread_.clear();
            balThreadRunning_ = false;
            return;
         }
      }
   };
   {
      std::unique_lock<std::mutex> lock(balThrMutex_);
      cbBalThread_.emplace_back(std::move(cb));
   }
   if (!balThreadRunning_) {
      balThreadRunning_ = true;
      std::thread(thrBalAvail).detach();
   }
}

void Wallet::onRefresh(const std::vector<BinaryData> &ids, bool online)
{
   for (const auto &id : ids) {
      if (id == regId_) {
         regId_.clear();
         init();

         const auto &cbTrackAddrChain = [this](bs::sync::SyncState st) {
            if (wct_) {
               wct_->walletReady(walletId());
            }
         };
         bs::sync::Wallet::init();
         getAddressTxnCounts([this, cbTrackAddrChain] {
            trackChainAddressUse(cbTrackAddrChain);
         });
      }
   }
}

std::vector<std::string> Wallet::registerWallet(const std::shared_ptr<ArmoryConnection> &armory, bool asNew)
{
   setArmory(armory);

   if (armory_) {
      const auto &cbRegister = [this](const std::string &)
      {
         logger_->debug("[bs::sync::Wallet::registerWallet] Wallet ready: {}", walletId());
         isRegistered_ = true;
      };

      const auto wallet = armory_->instantiateWallet(walletId());
      regId_ = armory_->registerWallet(wallet
         , walletId(), walletId(), getAddrHashes(), cbRegister, asNew);
      logger_->debug("[bs::sync::Wallet::registerWallet] register wallet {}, {} addresses = {}"
         , walletId(), getAddrHashes().size(), regId_);
      return { regId_ };
   }
   else {
      logger_->error("[bs::sync::Wallet::registerWallet] no armory");
   }
   return {};
}

void Wallet::unregisterWallet()
{
   historyCache_.clear();
}

void Wallet::init(bool force)
{
   if (!firstInit_ || force) {
      auto cbCounter = std::make_shared<std::atomic_int>(2);
      const auto &cbBalTxN = [this, cbCounter] {
         (*cbCounter)--;
         if ((*cbCounter <= 0)) {
            if (wct_) {
               wct_->balanceUpdated(walletId());
            }
         }
      };
      updateBalances(cbBalTxN);
      getAddressTxnCounts(cbBalTxN);
      firstInit_ = true;
   }
}

bs::core::wallet::TXSignRequest wallet::createTXRequest(const std::string &walletId
   , const std::vector<UTXO> &inputs
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients
   , const std::function<bs::Address(std::string &)> &cbChangeAddr
   , const uint64_t fee, bool isRBF, const uint64_t &origFee)
{
   bs::core::wallet::TXSignRequest request;
   request.walletIds = { walletId };

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
   request.fee = fee;

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
      auto promPtr = std::make_shared<std::promise<bs::Address>>();
      auto fut = promPtr->get_future();
      const auto &cbAddr = [this, &index, promPtr](const bs::Address &addr) {
         setAddressComment(addr, wallet::Comment::toString(wallet::Comment::ChangeAddress));
         try {
            index = getAddressIndex(addr);
         }
         catch (const std::exception &e) {
            logger_->error("[sync::Wallet::createTXRequest] failed to get {} index: {}"
               , addr.display(), e.what());
         }
         promPtr->set_value(addr);
      };
      getNewChangeAddress(cbAddr);
      return fut.get();
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
         utxo.witnessDataSizeBytes_ = unsigned(scrAddr.getWitnessDataSize());
         utxo.isInputSW_ = (scrAddr.getWitnessDataSize() != UINT32_MAX);
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
   request.walletIds = { walletId() };
//   request.wallet = this;
   request.populateUTXOs = true;
   Signer signer;
   if (!prevPart.isNull()) {
      signer.deserializeState(prevPart);
      if (feePerByte > 0) {
         bs::CheckRecipSigner chkSigner(prevPart);
         fee += chkSigner.estimateFee(feePerByte);
         fee -= 10 * feePerByte;    // subtract TX header size as it's counted twice
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
      signer.addRecipient(changeAddress.getRecipient(bs::XBTAmount{ changeVal }));
      request.change.value = changeVal;
      request.change.address = changeAddress;
      request.change.index = getAddressIndex(changeAddress);
   }

   request.prevStates.emplace_back(signer.serializeState());
   return request;
}

void WalletACT::onLedgerForAddress(const bs::Address &addr
   , const std::shared_ptr<AsyncClient::LedgerDelegate> &ld)
{
   std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> cb = nullptr;
   {
      std::unique_lock<std::mutex> lock(parent_->balanceData_->cbMutex);
      const auto &itCb = parent_->cbLedgerByAddr_.find(addr);
      if (itCb == parent_->cbLedgerByAddr_.end()) {
         return;
      }
      cb = itCb->second;
      parent_->cbLedgerByAddr_.erase(itCb);
   }
   if (cb) {
      cb(ld);
   }
}

bool Wallet::getLedgerDelegateForAddress(const bs::Address &addr
   , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &cb)
{
   if (!armory_) {
      return false;
   }
   {
      std::unique_lock<std::mutex> lock(balanceData_->cbMutex);
      const auto &itCb = cbLedgerByAddr_.find(addr);
      if (itCb != cbLedgerByAddr_.end()) {
         logger_->error("[bs::sync::Wallet::getLedgerDelegateForAddress] ledger callback for addr {} already exists", addr.display());
         return false;
      }
      cbLedgerByAddr_[addr] = cb;
   }
   return armory_->getLedgerDelegateForAddress(walletId(), addr);
}

int Wallet::addAddress(const bs::Address &addr, const std::string &index
   , bool sync)
{
   if (!addr.isNull()) {
      usedAddresses_.push_back(addr);
   }

   if (sync && signContainer_) {
      std::string idxCopy = index;
      if (idxCopy.empty() && !addr.isNull()) {
         idxCopy = getAddressIndex(addr);
         if (idxCopy.empty()) {
            idxCopy = addr.display();
         }
      }
      signContainer_->syncNewAddress(walletId(), idxCopy, nullptr);
   }

   return (usedAddresses_.size() - 1);
}

void Wallet::syncAddresses()
{
   if (armory_) {
      registerWallet();
   }

   if (signContainer_) {
      std::set<BinaryData> addrSet;
      for (const auto &addr : getUsedAddressList()) {
         addrSet.insert(addr.id());
      }
      signContainer_->syncAddressBatch(walletId(), addrSet, [](bs::sync::SyncState) {});
   }
}

void Wallet::newAddresses(const std::vector<std::string> &inData
   , const CbAddresses &cb)
{
   if (signContainer_) {
      signContainer_->syncNewAddresses(walletId(), inData, cb);
   } else {
      if (logger_) {
         logger_->error("[bs::sync::Wallet::newAddresses] no signer set");
      }
   }
}

void Wallet::trackChainAddressUse(const std::function<void(bs::sync::SyncState)> &cb)
{
   if (!signContainer_) {
      cb(bs::sync::SyncState::NothingToDo);
      return;
   }
   //1) round up all addresses that have a tx count
   std::set<BinaryData> usedAddrSet;
   for (auto& addrPair : balanceData_->addressTxNMap) {
      if (addrPair.second != 0) {
         usedAddrSet.insert(addrPair.first);
      }
   }
   for (auto& addrPair : balanceData_->addressBalanceMap) {
      if (usedAddrSet.find(addrPair.first) != usedAddrSet.end()) {
         continue;   // skip already added addresses
      }
      if (!addrPair.second.empty()) {
         bool hasBalance = false;
         for (int i = 0; i < 3; ++i) {
            if (addrPair.second[i] > 0) {
               hasBalance = true;
               break;
            }
         }
         if (hasBalance) {
            usedAddrSet.insert(addrPair.first);
         }
      }
   }

   logger_->debug("[bs::sync::Wallet::trackChainAddressUse] {}: {} used address[es]", walletId(), usedAddrSet.size());
   //2) send to armory wallet for processing
   signContainer_->syncAddressBatch(walletId(), usedAddrSet, cb);
}

size_t Wallet::getActiveAddressCount()
{
   std::unique_lock<std::mutex> lock(balanceData_->addrMapsMtx);

   size_t count = 0;
   for (auto& addrBal : balanceData_->addressBalanceMap) {
      if (addrBal.second[0] != 0) {
         ++count;
      }
   }
   return count;
}
