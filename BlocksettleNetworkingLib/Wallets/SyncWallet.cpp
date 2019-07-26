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
{}

Wallet::~Wallet()
{
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
      netType_ = data.netType;
      for (const auto &addr : data.addresses) 
      {
         addAddress(addr.address, addr.index, addr.address.getType(), false);
         setAddressComment(addr.address, addr.comment, false);
      }

      for (const auto &txComment : data.txComments)
         setTransactionComment(txComment.txHash, txComment.comment, false);

      if (cbDone)
         cbDone();
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
   if (wct_) {
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

std::vector<uint64_t> Wallet::getAddrBalance(const bs::Address &addr) const
{
   if (!isBalanceAvailable())
      throw std::runtime_error("uninitialized db connection");

   std::unique_lock<std::mutex> lock(addrMapsMtx_);

   auto iter = addressBalanceMap_.find(addr.prefixed());
   if (iter == addressBalanceMap_.end())
      return {};

   return iter->second;
}

uint64_t Wallet::getAddrTxN(const bs::Address &addr) const
{
   if (!isBalanceAvailable())
      throw std::runtime_error("uninitialized db connection");

   std::unique_lock<std::mutex> lock(addrMapsMtx_);

   auto iter = addressTxNMap_.find(addr.prefixed());
   if (iter == addressTxNMap_.end())
      return 0;

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
////
//// Combined DB fetch methods
////
////////////////////////////////////////////////////////////////////////////////
void WalletACT::onCombinedBalances(const std::map<std::string, CombinedBalances> &balanceMap)
{
   bool ourUpdate = false;
   BTCNumericTypes::balance_type total = 0, spendable = 0, unconfirmed = 0;
   uint64_t addrCount = 0;
   for (const auto &wltBal : balanceMap) {
      if (!parent_->isOwnId(wltBal.first)) {
         continue;
      }
      ourUpdate = true;

      total += static_cast<BTCNumericTypes::balance_type>(
         wltBal.second.walletBalanceAndCount_[0]) / BTCNumericTypes::BalanceDivider;
/*      spendable += static_cast<BTCNumericTypes::balance_type>(
         wltBal.second.walletBalanceAndCount_[1]) / BTCNumericTypes::BalanceDivider;*/
      unconfirmed += static_cast<BTCNumericTypes::balance_type>(
         wltBal.second.walletBalanceAndCount_[2]) / BTCNumericTypes::BalanceDivider;

      //wallet txn count
      addrCount += wltBal.second.walletBalanceAndCount_[3];

      //address balances
      parent_->updateMap<std::map<BinaryData, std::vector<uint64_t>>>(
         wltBal.second.addressBalances_, parent_->addressBalanceMap_);
   }
   spendable = total - unconfirmed;

   if (ourUpdate) {
      parent_->totalBalance_ = total;
      parent_->spendableBalance_ = spendable;
      parent_->unconfirmedBalance_ = unconfirmed;
      parent_->addrCount_ = addrCount;

      std::unique_lock<std::mutex> lock(parent_->cbMutex_);
      for (const auto &cb : parent_->cbBalances_) {
         if (cb) {
            cb();
         }
      }
      parent_->cbBalances_.clear();
   }
}

bool Wallet::updateBalances(const std::function<void(void)> &cb)
{  /***
   The callback is only used to signify request completion, use the
   get methods to grab the individual balances
   ***/

   size_t cbSize = 0;
   {
      std::unique_lock<std::mutex> lock(cbMutex_);
      cbSize = cbBalances_.size();
      cbBalances_.push_back(cb);
   }
   if (cbSize == 0) {
      std::vector<std::string> walletIDs;
      walletIDs.push_back(walletId());
      try {
         walletIDs.push_back(walletIdInt());
      } catch (std::exception&) {}
      return armory_->getCombinedBalances(walletIDs);
   } else {          // if the callbacks queue is not empty, don't call
      return true;   // armory's RPC - just add the callback and return
   }
}

bool Wallet::getSpendableTxOutList(const ArmoryConnection::UTXOsCb &cb, uint64_t val)
{
   //combined utxo fetch method

   if (!isBalanceAvailable())
      return false;

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

   armory_->getSpendableTxOutListForValue(walletIDs, val, cbTxOutList);
   return true;
}

bool Wallet::getSpendableZCList(const ArmoryConnection::UTXOsCb &cb) const
{
   if (!isBalanceAvailable())
      return false;

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
   if (!isBalanceAvailable())
      return false;

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

void WalletACT::onCombinedTxnCounts(const std::map<std::string, CombinedCounts> &countMap)
{
   bool ourUpdate = false;
   for (const auto &count : countMap) {
      if (!parent_->isOwnId(count.first)) {
         continue;
      }
      ourUpdate = true;
      parent_->updateMap<std::map<BinaryData, uint64_t>>(
         count.second.addressTxnCounts_, parent_->addressTxNMap_);
   }
   if (ourUpdate) {
      std::unique_lock<std::mutex> lock(parent_->cbMutex_);
      for (const auto &cb : parent_->cbTxNs_) {
         if (cb) {
            cb();
         }
      }
      parent_->cbTxNs_.clear();
   }
}

bool Wallet::getAddressTxnCounts(std::function<void(void)> cb)
{  /***
   Same as updateBalances, this methods grabs the addr txn count
   for all addresses in wallet (inner chain included) and caches
   them locally.

   Use getAddressTxnCount to get a specific count for a given
   address from the cache.
   ***/
   size_t cbSize = 0;
   {
      std::unique_lock<std::mutex> lock(cbMutex_);
      cbSize = cbTxNs_.size();
      cbTxNs_.push_back(cb);
   }
   if (cbSize == 0) {
      std::vector<std::string> walletIDs;
      walletIDs.push_back(walletId());
      try {
         walletIDs.push_back(walletIdInt());
      } catch (std::exception&) {}
      return armory_->getCombinedTxNs(walletIDs);
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
      if(act_ == nullptr)
         act_ = make_unique<WalletACT>(armory_.get(), this);
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

   const auto &cbTX = [this](const Tx &tx) {
      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         const TxIn in = tx.getTxInCopy(i);
         const OutPoint op = in.getOutPoint();

         const auto &cbPrevTX = [this, idx=op.getTxOutIndex()](const Tx &prevTx) {
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
               std::unique_lock<std::mutex> lock(addrMapsMtx_);
               const auto &itTxn = addressTxNMap_.find(addr.id());
               if (itTxn != addressTxNMap_.end()) {
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
}

void Wallet::onNewBlock(unsigned int depth)
{
   logger_->debug("[{}]", __func__);
   init(true);
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
         logger_->debug("Wallet ready: {}", walletId());
         isRegistered_ = true;
      };

      const auto wallet = armory_->instantiateWallet(walletId());
      regId_ = armory_->registerWallet(wallet
         , walletId(), walletId(), getAddrHashes(), cbRegister, asNew);
      logger_->debug("[{}] register wallet {}, {} addresses = {}"
         , __func__, walletId(), getAddrHashes().size(), regId_);
      return { regId_ };
   }
   else {
      logger_->error("[{}] no armory", __func__);
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

void WalletACT::onLedgerForAddress(const bs::Address &addr
   , const std::shared_ptr<AsyncClient::LedgerDelegate> &ld)
{
   std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> cb = nullptr;
   {
      std::unique_lock<std::mutex> lock(parent_->cbMutex_);
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
      std::unique_lock<std::mutex> lock(cbMutex_);
      const auto &itCb = cbLedgerByAddr_.find(addr);
      if (itCb != cbLedgerByAddr_.end()) {
         logger_->error("[{}] ledger callback for addr {} already exists", __func__, addr.display());
         return false;
      }
      cbLedgerByAddr_[addr] = cb;
   }
   return armory_->getLedgerDelegateForAddress(walletId(), addr);
}

int Wallet::addAddress(const bs::Address &addr, const std::string &index
   , AddressEntryType aet, bool sync)
{
   if (!addr.isNull())
      usedAddresses_.push_back(addr);

   if (sync && signContainer_) 
   {
      std::string idxCopy = index;
      if (idxCopy.empty() && !addr.isNull()) 
      {
         aet = addr.getType();
         idxCopy = getAddressIndex(addr);
         if (idxCopy.empty()) 
            idxCopy = addr.display();
      }

      signContainer_->syncNewAddress(walletId(), idxCopy, aet, nullptr);
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

void Wallet::newAddresses(
   const std::vector<std::pair<std::string, AddressEntryType>> &inData
   , const CbAddresses &cb)
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

void Wallet::trackChainAddressUse(std::function<void(bs::sync::SyncState)> cb)
{
   if (!signContainer_) {
      cb(bs::sync::SyncState::NothingToDo);
      return;
   }
   //1) round up all addresses that have a tx count
   std::set<BinaryData> usedAddrSet;
   for (auto& addrPair : addressTxNMap_) {
      if (addrPair.second != 0)
         usedAddrSet.insert(addrPair.first);
   }

   logger_->debug("[{}] {}: {} used address[es]", __func__, walletId(), usedAddrSet.size());
   //2) send to armory wallet for processing
   signContainer_->syncAddressBatch(walletId(), usedAddrSet, cb);
}

size_t Wallet::getActiveAddressCount()
{
   std::unique_lock<std::mutex> lock(addrMapsMtx_);
   
   size_t count = 0;
   for (auto& addrBal : addressBalanceMap_)
   {
      if (addrBal.second[0] != 0)
         ++count;
   }

   return count;
}
