#include "SyncHDLeaf.h"

#include <unordered_map>
#include <QLocale>
#include <QMutexLocker>
#include "CheckRecipSigner.h"
#include "SignContainer.h"

const uint32_t kExtConfCount = 6;
const uint32_t kIntConfCount = 1;

using namespace bs::sync;


hd::Leaf::Leaf(const std::string &walletId, const std::string &name, const std::string &desc
   , SignContainer *container, const std::shared_ptr<spdlog::logger> &logger
   , bs::core::wallet::Type type, bool extOnlyAddresses)
   : bs::sync::Wallet(container, logger)
   , walletId_(walletId), type_(type)
   , name_(name), desc_(desc), isExtOnly_(extOnlyAddresses)
   , rescanWalletId_(walletId + "_rescan")
{ }

hd::Leaf::~Leaf() = default;

void hd::Leaf::synchronize(const std::function<void()> &cbDone)
{
   const auto &cbProcess = [this, cbDone](bs::sync::WalletData data) {
      encryptionTypes_ = data.encryptionTypes;
      encryptionKeys_ = data.encryptionKeys;
      encryptionRank_ = data.encryptionRank;
      netType_ = data.netType;
      emit metaDataChanged();

      for (const auto &addr : data.addresses) {
         addAddress(addr.address, addr.index, addr.address.getType(), false);
         setAddressComment(addr.address, addr.comment, false);
      }
      for (const auto &addr : data.addrPool) {  //addPool normally won't contain comments
         const auto path = bs::hd::Path::fromString(addr.index);
         addressPool_[{ path, addr.address.getType() }] = addr.address;
         poolByAddr_[addr.address] = { path, addr.address.getType() };
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

void hd::Leaf::setArmory(const std::shared_ptr<ArmoryObject> &armory)
{
   bs::sync::Wallet::setArmory(armory);
   if (armory_) {
      connect(armory_.get(), &ArmoryObject::zeroConfReceived, this, &hd::Leaf::onZeroConfReceived, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryObject::refresh, this, &hd::Leaf::onRefresh, Qt::QueuedConnection);
   }
}

void hd::Leaf::init(const bs::hd::Path &path)
{
   if (path != path_) {
      path_ = path;
      suffix_.clear();
      suffix_ = bs::hd::Path::elemToKey(index());
      walletName_ = name_ + "/" + suffix_;
   }

   if (!path.length()) {
      reset();
   }
}

std::vector<BinaryData> hd::Leaf::getRegAddresses(const std::vector<PooledAddress> &src)
{
   std::vector<BinaryData> result;
   for (const auto &addr : src) {
      result.push_back(addr.second.prefixed());
   }
   return result;
}

void hd::Leaf::fillPortion(bs::hd::Path::Elem start, const std::function<void()> &cb, unsigned int size)
{
   currentPortion_.activeAddresses.clear();
   currentPortion_.addresses.clear();
   currentPortion_.addresses.reserve(size * 4);
   currentPortion_.poolKeyByAddr.clear();
   currentPortion_.start = start;
   currentPortion_.end = start + size - 1;

   std::vector<std::pair<std::string, AddressEntryType>> request;
   for (bs::hd::Path::Elem addrType : {0, 1}) {
      for (const auto aet : { AddressEntryType_P2SH, AddressEntryType_P2WPKH }) {
         for (bs::hd::Path::Elem i = start; i < start + size; i++) {
            bs::hd::Path addrPath({ addrType, i });
            request.push_back({ addrPath.toString(), aet });
         }
      }
   }
   const auto &cbAddrs = [this, cb](const std::vector<std::pair<bs::Address, std::string>> &addrs) {
      for (const auto &addr : addrs) {
         const auto path = bs::hd::Path::fromString(addr.second);
         currentPortion_.addresses.push_back({ {path, addr.first.getType()}, addr.first });
         currentPortion_.poolKeyByAddr[addr.first] = { path, addr.first.getType() };
      }
      if (cb) {
         cb();
      }
   };
   newAddresses(request, cbAddrs, false);
}

void hd::Leaf::scanAddresses(unsigned int startIdx, unsigned int portionSize
   , const std::function<void(const std::string &walletId, unsigned int idx)> &cbw)
{
   if (startIdx == UINT32_MAX) {    // special index to skip rescan for particular wallet
      onScanComplete();
      return;
   }
   const auto &cbPortion = [this]() {
      if (!armory_) {
         logger_->error("[sync::hd::Leaf::scanAddresses] {} armory is not set", walletId());
         return;
      }
      currentPortion_.registered = true;
      rescanRegId_ = armory_->registerWallet(rescanWallet_, rescanWalletId_
         , getRegAddresses(currentPortion_.addresses), nullptr, true);
   };

   portionSize_ = portionSize;
   cbWriteLast_ = cbw;
   fillPortion(startIdx, cbPortion, portionSize);
   if (cbWriteLast_) {
      cbWriteLast_(walletId_, startIdx);
   }
}

void hd::Leaf::processPortion()
{
   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }

   processing_ = (int)currentPortion_.start;
   currentPortion_.registered = false;

   const auto &cbProcess = [this] {
      if (!currentPortion_.activeAddresses.empty()) {
         std::sort(currentPortion_.activeAddresses.begin(), currentPortion_.activeAddresses.end()
            , [](const PooledAddress &a, const PooledAddress &b)
         { return (a.first.path < b.first.path) && (a.first.aet < b.first.aet); });
         onSaveToWallet(currentPortion_.activeAddresses);

         if (cbWriteLast_) {
            cbWriteLast_(walletId_, currentPortion_.end + 1);
         }

         const auto &cbPortion = [this]() {
            currentPortion_.registered = true;
            rescanRegId_ = armory_->registerWallet(rescanWallet_
               , rescanWalletId_, getRegAddresses(currentPortion_.addresses), {});
         };
         fillPortion(currentPortion_.end + 1, cbPortion, portionSize_);
      }
      else {
         currentPortion_.start = currentPortion_.end = 0;
         currentPortion_.addresses.clear();
         processing_ = -1;
         rescanWallet_.reset();

         if (cbWriteLast_) {
            cbWriteLast_(walletId_, UINT32_MAX);
         }
         onScanComplete();
      }
   };

   const auto &cbTXs = [this, cbProcess](const std::vector<Tx> &txs) {
      std::set<BinaryData> opTxHashes;
      std::map<BinaryData, std::set<uint32_t>> opTxIndices;

      for (const auto &tx : txs) {
         for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
            TxOut out = tx.getTxOutCopy((int)i);
            const auto addr = bs::Address::fromTxOut(out);
            const auto &itAddr = currentPortion_.poolKeyByAddr.find(addr);
            if (itAddr != currentPortion_.poolKeyByAddr.end()) {
               currentPortion_.activeAddresses.push_back({ itAddr->second, itAddr->first });
            }
         }
         for (size_t i = 0; i < tx.getNumTxIn(); i++) {
            auto in = tx.getTxInCopy((int)i);
            OutPoint op = in.getOutPoint();
            opTxHashes.insert(op.getTxHash());
            opTxIndices[op.getTxHash()].insert(op.getTxOutIndex());
         }
      }

      const auto &cbInputs = [this, opTxIndices, cbProcess](const std::vector<Tx> &inputs) {
         for (const auto &prevTx : inputs) {
            const auto &itIdx = opTxIndices.find(prevTx.getThisHash());
            if (itIdx == opTxIndices.end()) {
               continue;
            }
            for (const auto txOutIdx : itIdx->second) {
               const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy(txOutIdx));
               const auto &itAddr = currentPortion_.poolKeyByAddr.find(addr);
               if (itAddr != currentPortion_.poolKeyByAddr.end()) {
                  currentPortion_.activeAddresses.push_back({ itAddr->second, itAddr->first });
               }
            }
         }
         cbProcess();
      };
      armory_->getTXsByHash(opTxHashes, cbInputs);
   };

   const auto &cbHistory = [this, cbTXs, cbProcess](std::vector<ClientClasses::LedgerEntry> entries) {
      if (entries.empty()) {
         cbProcess();
      }
      else {
         std::set<BinaryData> txHashes;
         for (const auto &entry : entries) {
            txHashes.insert(entry.getTxHash());
         }
         armory_->getTXsByHash(txHashes, cbTXs);
      }
   };
   armory_->getWalletsHistory({ rescanWalletId_ }, cbHistory);
}

void hd::Leaf::onZeroConfReceived(const std::vector<bs::TXEntry>)
{
//!   activateAddressesFromLedger(armory_->getZCentries(reqId));
}  // if ZC is received, then likely wallet already contains the participating address

void hd::Leaf::onRefresh(std::vector<BinaryData> ids, bool online)
{
   const auto &cbRegisterExt = [this, online] {
      if (isExtOnly_ || (regIdExt_.empty() && regIdInt_.empty())) {
         emit walletReady(QString::fromStdString(walletId()));
         if (online) {
            postOnline();
         }
      }
   };
   const auto &cbRegisterInt = [this, online] {
      if (regIdExt_.empty() && regIdInt_.empty()) {
         emit walletReady(QString::fromStdString(walletId()));
         if (online) {
            postOnline();
         }
      }
   };

   if (!regIdExt_.empty() || !regIdInt_.empty()) {
      for (const auto &id : ids) {
         if (id.isNull()) {
            continue;
         }
         if (id == regIdExt_) {
            regIdExt_.clear();
            cbRegisterExt();
         } else if (id == regIdInt_) {
            regIdInt_.clear();
            cbRegisterInt();
         }
      }
   }

   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }
   const auto &it = std::find(ids.begin(), ids.end(), rescanRegId_);
   if (it == ids.end()) {
      return;
   }
   processPortion();
}

void hd::Leaf::postOnline()
{
   if (btcWallet_) {
      btcWallet_->setUnconfirmedTarget(kExtConfCount);
   }
   if (btcWalletInt_) {
      btcWalletInt_->setUnconfirmedTarget(kIntConfCount);
   }
   bs::sync::Wallet::firstInit();
}

void hd::Leaf::firstInit(bool force)
{
   if (!armory_ || (armory_->state() != ArmoryConnection::State::Ready)) {
      return;
   }
   postOnline();

   if (activateAddressesInvoked_ || !armory_) {
      return;
   }
   const auto &cb = [this](std::vector<ClientClasses::LedgerEntry> entries) {
      activateAddressesFromLedger(entries);
   };
   activateAddressesInvoked_ = true;
   armory_->getWalletsHistory({ walletId() }, cb);
}

void hd::Leaf::activateAddressesFromLedger(const std::vector<ClientClasses::LedgerEntry> &led)
{
   std::set<BinaryData> txHashes;
   for (const auto &entry : led) {
      txHashes.insert(entry.getTxHash());
   }
   const auto &cb = [this](const std::vector<Tx> &txs) {
      auto activated = std::make_shared<bool>(false);
      std::set<BinaryData> opTxHashes;
      std::map<BinaryData, std::set<uint32_t>> opTxIndices;

      for (const auto &tx : txs) {
         for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
            TxOut out = tx.getTxOutCopy((int)i);
            const auto addr = bs::Address::fromTxOut(out);
            if (containsHiddenAddress(addr)) {
               *activated = true;
               activateHiddenAddress(addr);
            }
         }
         for (size_t i = 0; i < tx.getNumTxIn(); i++) {
            auto in = tx.getTxInCopy((int)i);
            OutPoint op = in.getOutPoint();
            opTxHashes.insert(op.getTxHash());
            opTxIndices[op.getTxHash()].insert(op.getTxOutIndex());
         }
      }
      const auto &cbInputs = [this, activated, opTxIndices](const std::vector<Tx> &inputs) {
         for (const auto &prevTx : inputs) {
            const auto &itIdx = opTxIndices.find(prevTx.getThisHash());
            if (itIdx == opTxIndices.end()) {
               continue;
            }
            for (const auto txOutIdx : itIdx->second) {
               const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy(txOutIdx));
               if (containsHiddenAddress(addr)) {
                  *activated = true;
                  activateHiddenAddress(addr);
               }
            }
         }
         if (*activated) {
            emit addressAdded();
         }
      };
      if (!opTxHashes.empty()) {
         armory_->getTXsByHash(opTxHashes, cbInputs);
      }
   };
   if (!txHashes.empty()) {
      armory_->getTXsByHash(txHashes, cb);
   }
}

void hd::Leaf::activateHiddenAddress(const bs::Address &addr)
{
   const auto itAddr = poolByAddr_.find(addr);
   if (itAddr == poolByAddr_.end()) {
      return;
   }
   createAddressWithPath(itAddr->second, false);
}

void hd::Leaf::reset()
{
   lastIntIdx_ = lastExtIdx_ = 0;
   addressMap_.clear();
   usedAddresses_.clear();
   intAddresses_.clear();
   extAddresses_.clear();
   addrToIndex_.clear();
   addrPrefixedHashes_.clear();
   addressPool_.clear();
   poolByAddr_.clear();
   emit walletReset();
}

std::string hd::Leaf::walletId() const
{
   return walletId_;
}

std::string hd::Leaf::description() const
{
   return desc_;
}

bool hd::Leaf::containsAddress(const bs::Address &addr)
{
   return !getAddressIndex(addr).empty();
}

bool hd::Leaf::containsHiddenAddress(const bs::Address &addr) const
{
   return (poolByAddr_.find(addr) != poolByAddr_.end());
}

// Return an external-facing address.
bs::Address hd::Leaf::getNewExtAddress(AddressEntryType aet, const CbAddress &cb)
{
   return createAddress(aet, cb, false);
}

// Return an internal-facing address.
bs::Address hd::Leaf::getNewIntAddress(AddressEntryType aet, const CbAddress &cb)
{
   if (isExtOnly_) {
      if (cb) {
         cb({});
      }
      return {};
   }
   return createAddress(aet, cb, true);
}

// Return a change address.
bs::Address hd::Leaf::getNewChangeAddress(AddressEntryType aet, const CbAddress &cb)
{
   return createAddress(aet, cb, isExtOnly_ ? false : true);
}

bs::Address hd::Leaf::getRandomChangeAddress(AddressEntryType aet, const CbAddress &cb)
{
   if (isExtOnly_) {
      if (extAddresses_.empty()) {
         return getNewExtAddress(aet, cb);
      } else if (extAddresses_.size() == 1) {
         return extAddresses_[0];
      }
      return extAddresses_[qrand() % extAddresses_.size()];
   }
   else {
      if (!lastIntIdx_) {
         return getNewChangeAddress(aet, cb);
      }
      else {
         return intAddresses_[qrand() % intAddresses_.size()];
      }
   }
}

std::vector<BinaryData> hd::Leaf::getAddrHashes() const
{
   std::vector<BinaryData> result;

   const auto addrsExt = getAddrHashesExt();
   const auto addrsInt = getAddrHashesInt();

   result.insert(result.end(), addrsExt.cbegin(), addrsExt.cend());
   result.insert(result.end(), addrsInt.cbegin(), addrsInt.cend());

   return result;
}

std::vector<BinaryData> hd::Leaf::getAddrHashesExt() const
{
   std::vector<BinaryData> result;
   result.insert(result.end(), addrPrefixedHashes_.external.cbegin(), addrPrefixedHashes_.external.cend());
   for (const auto &addr : addressPool_) {
      const auto path = addr.first.path;
      if (path.get(-2) == addrTypeExternal) {
         result.push_back(addr.second.id());
      }
   }
   return result;
}

std::vector<BinaryData> hd::Leaf::getAddrHashesInt() const
{
   std::vector<BinaryData> result;
   result.insert(result.end(), addrPrefixedHashes_.internal.cbegin(), addrPrefixedHashes_.internal.cend());
   for (const auto &addr : addressPool_) {
      const auto path = addr.first.path;
      if (path.get(-2) == addrTypeInternal) {
         result.push_back(addr.second.id());
      }
   }
   return result;
}

std::vector<std::string> hd::Leaf::registerWallet(const std::shared_ptr<ArmoryObject> &armory, bool asNew)
{
   setArmory(armory);

   if (armory_) {
      const auto addrsExt = getAddrHashesExt();
      std::vector<std::string> regIds;
      const auto &cbEmpty = [](const std::string &) {};

      regIdExt_ = armory_->registerWallet(btcWallet_, walletId()
         , addrsExt, cbEmpty, asNew);
      regIds.push_back(regIdExt_);

      if (!isExtOnly_) {
         const auto addrsInt = getAddrHashesInt();
         regIdInt_ = armory_->registerWallet(btcWalletInt_
            , getWalletIdInt(), addrsInt, cbEmpty, asNew);
         regIds.push_back(regIdInt_);
      }
      return regIds;
   }
   return {};
}

void hd::Leaf::unregisterWallet()
{
   Wallet::unregisterWallet();
   if (armory_) {
      armory_->registerWallet(btcWalletInt_, getWalletIdInt(), {}, [](const std::string &){}, false);
   }
   btcWalletInt_.reset();
}

bs::Address hd::Leaf::createAddress(AddressEntryType aet, const CbAddress &cb, bool isInternal)
{
   bs::hd::Path addrPath;
   if (isInternal) {
      if (isExtOnly_) {
         if (cb) {
            cb({});
         }
         return {};
      }
      addrPath.append(addrTypeInternal);
      addrPath.append(lastIntIdx_++);
   }
   else {
      addrPath.append(addrTypeExternal);
      addrPath.append(lastExtIdx_++);
   }
   return createAddress({ addrPath, aet }, cb);
}

bs::Address hd::Leaf::createAddress(const AddrPoolKey &key, const CbAddress &cb, bool signal)
{
   const bool isInternal = (key.path.get(-2) == addrTypeInternal);
   if (isInternal && isExtOnly_) {
      if (cb) {
         cb({});
      }
      return {};
   }
   bs::Address result;

   AddrPoolKey keyCopy = key;
   if (key.aet == AddressEntryType_Default) {
      keyCopy.aet = defaultAET_;
   }
   const auto addrPoolIt = addressPool_.find(keyCopy);
   if (addrPoolIt != addressPool_.end()) {
      result = std::move(addrPoolIt->second);
      addressPool_.erase(addrPoolIt->first);
      poolByAddr_.erase(result);
   }
   else {
      const auto &cbPool = [this, cb, keyCopy] {
         const auto addrPoolIt = addressPool_.find(keyCopy);
         if (addrPoolIt != addressPool_.end()) {
            const auto addr = addrPoolIt->second;
            addressPool_.erase(addrPoolIt->first);
            poolByAddr_.erase(addr);
            if (cb) {
               cb(addr);
            }
         }
         else {
            logger_->warn("[createAddress] failed to find {} in the pool ({}) after top-up"
               , keyCopy.path.toString(), addressPool_.size());
            if (cb) {
               cb({});
            }
         }
      };
      topUpAddressPool(cbPool);
   }

   if (addrToIndex_.find(result.unprefixed()) != addrToIndex_.end()) {
      return result;
   }

   addAddress(result, key.path.toString(), keyCopy.aet);

   if (signal) {
      emit addressAdded();
   }
   return result;
}

void hd::Leaf::topUpAddressPool(const std::function<void()> &cb, size_t nbIntAddresses, size_t nbExtAddresses)
{
   const size_t nbPoolInt = nbIntAddresses ? 0 : getLastAddrPoolIndex(addrTypeInternal) - lastIntIdx_ + 1;
   const size_t nbPoolExt = nbExtAddresses ? 0 : getLastAddrPoolIndex(addrTypeExternal) - lastExtIdx_ + 1;
   nbIntAddresses = qMax(nbIntAddresses, intAddressPoolSize_);
   nbExtAddresses = qMax(nbExtAddresses, extAddressPoolSize_);

   std::vector<std::pair<std::string, AddressEntryType>> request;
   for (const auto aet : poolAET_) {
      if (!isExtOnly_ && (nbPoolInt < (intAddressPoolSize_ / 2))) {
         for (bs::hd::Path::Elem i = lastIntIdx_; i < lastIntIdx_ + nbIntAddresses; i++) {
            bs::hd::Path addrPath({ addrTypeExternal, i });
            request.push_back({ addrPath.toString(), aet });
         }
      }
      if (nbPoolExt < (extAddressPoolSize_ / 2)) {
         for (bs::hd::Path::Elem i = lastExtIdx_; i < lastExtIdx_ + nbExtAddresses; i++) {
            bs::hd::Path addrPath({ addrTypeExternal, i });
            request.push_back({ addrPath.toString(), aet });
         }
      }
   }

   const auto &cbAddrs = [this, cb](const std::vector<std::pair<bs::Address, std::string>> &addrs) {
      for (const auto &addr : addrs) {
         const auto path = bs::hd::Path::fromString(addr.second);
         addressPool_[{path, addr.first.getType()}] = addr.first;
         poolByAddr_[addr.first] = { path, addr.first.getType() };
      }
      if (cb) {
         cb();
      }
   };
   newAddresses(request, cbAddrs, false);
}

hd::Leaf::AddrPoolKey hd::Leaf::getAddressIndexForAddr(const BinaryData &addr) const
{
   bs::Address p2pk(addr, AddressEntryType_P2PKH);
   bs::Address p2sh(addr, AddressEntryType_P2SH);
   AddrPoolKey index;
   for (const auto &bd : { p2pk.unprefixed(), p2sh.unprefixed() }) {
      const auto itIndex = addrToIndex_.find(bd);
      if (itIndex != addrToIndex_.end()) {
         index = itIndex->second;
         break;
      }
   }
   return index;
}

hd::Leaf::AddrPoolKey hd::Leaf::addressIndex(const bs::Address &addr) const
{
   const auto itIndex = addrToIndex_.find(addr.unprefixed());
   if (itIndex == addrToIndex_.end()) {
      return {};
   }
   return itIndex->second;
}

bs::hd::Path hd::Leaf::getPathForAddress(const bs::Address &addr) const
{
   const auto index = addressIndex(addr);
   if (index.empty()) {
      const auto &itPool = poolByAddr_.find(addr);
      if (itPool == poolByAddr_.end()) {
         return {};
      }
      return itPool->second.path;
   }
   if (index.path.length() < 2) {
      return {};
   }
   return index.path;
}

bool hd::Leaf::getLedgerDelegateForAddress(const bs::Address &addr
   , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &cb
   , QObject *context)
{
   if (armory_) {
      const auto path = getPathForAddress(addr);
      if (path.get(-2) == addrTypeInternal) {
         return armory_->getLedgerDelegateForAddress(getWalletIdInt()
            , addr, cb, context);
      }
      else {
         return armory_->getLedgerDelegateForAddress(walletId(), addr, cb, context);
      }
   }
   return false;
}

std::string hd::Leaf::getWalletIdInt() const
{
   if (walletIdInt_.empty()) {
      for (const auto &c : walletId()) {
         if (isupper(c)) {
            walletIdInt_.push_back(tolower(c));
         } else if (islower(c)) {
            walletIdInt_.push_back(toupper(c));
         } else {
            walletIdInt_.push_back(c);
         }
      }
   }
   return walletIdInt_;
}

bool hd::Leaf::hasId(const std::string &id) const
{
   return ((walletId() == id) || (getWalletIdInt() == id));
}

int hd::Leaf::addAddress(const bs::Address &addr, const std::string &index, AddressEntryType aet, bool sync)
{
   const auto path = bs::hd::Path::fromString(index);
   const bool isInternal = (path.get(-2) == addrTypeInternal);
   const int id = bs::sync::Wallet::addAddress(addr, index, aet, sync);
   const auto addrIndex = path.get(-1);
   if (isInternal) {
      intAddresses_.push_back(addr);
      addrPrefixedHashes_.internal.insert(addr.id());
      if (addrIndex >= lastIntIdx_) {
         lastIntIdx_ = addrIndex + 1;
      }
   } else {
      extAddresses_.push_back(addr);
      addrPrefixedHashes_.external.insert(addr.id());
      if (addrIndex >= lastExtIdx_) {
         lastExtIdx_ = addrIndex + 1;
      }
   }
   addressMap_[{path, aet}] = addr;
   addrToIndex_[addr.unprefixed()] = {path, aet};
   return id;
}

void hd::Leaf::updateBalances(const std::function<void(std::vector<uint64_t>)> &cb)
{
   if (!isBalanceAvailable()) {
      return;
   }
   auto prevBalances = std::make_shared<std::vector<uint64_t>>();

   const auto &cbBalances = [this, cb, prevBalances]
   (ReturnMessage<std::vector<uint64_t>> balanceVector)->void {
      try {
         auto bv = balanceVector.get();
         if (bv.size() < 4) {
            return;
         }
         if (prevBalances->size() == 4) {
            for (size_t i = 0; i < 4; ++i) {
               bv[i] += (*prevBalances)[i];
            }
         }

         const auto &cbTxOutList = [this, bv] (std::vector<UTXO> inputs) {
            spendableBalanceCorrection_ = 0;
            for (const auto &input : inputs) {
               if (armory_->getConfirmationsNumber(input.getHeight()) >= kExtConfCount) {
                  continue;
               }
               const auto addr = bs::Address::fromUTXO(input);
               if (!isExternalAddress(addr)) {
                  continue;
               }
               spendableBalanceCorrection_ += input.getValue() / BTCNumericTypes::BalanceDivider;
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
         };
         Wallet::getSpendableTxOutList(cbTxOutList, this);

         if (cb) {
            cb(bv);
         }
      } catch (const std::exception &e) {
         if (logger_) {
            logger_->error("[hd::Leaf::UpdateBalances] Return data error " \
               "- {}", e.what());
         }
      }
   };

   const auto &cbBalancesExt = [this, cbBalances, prevBalances]
      (ReturnMessage<std::vector<uint64_t>> balanceVector)->void {
      try {
         auto bv = balanceVector.get();
         prevBalances->insert(prevBalances->end(), bv.cbegin(), bv.cend());
         btcWalletInt_->getBalancesAndCount(armory_->topBlock(), cbBalances);
      }
      catch (const std::exception &e) {
         if (logger_) {
            logger_->error("[hd::Leaf::UpdateBalances] Return data error " \
               "- {}", e.what());
         }
      }
   };

   if (isExtOnly_) {
      btcWallet_->getBalancesAndCount(armory_->topBlock(), cbBalances);
   }
   else {
      btcWallet_->getBalancesAndCount(armory_->topBlock(), cbBalancesExt);
   }
}

bool hd::Leaf::getAddrBalance(const bs::Address &addr, std::function<void(std::vector<uint64_t>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   static const std::vector<uint64_t> defVal = { 0, 0, 0 };

   if (updateAddrBalance_) {
      auto cbCnt = std::make_shared<std::atomic_uint>(0);
      const auto &cbAddrBalance = [this, cbCnt]
         (ReturnMessage<std::map<BinaryData, std::vector<uint64_t>>> balanceMap) {
         try {
            const auto bm = balanceMap.get();
            updateMap<std::map<BinaryData, std::vector<uint64_t>>>(bm, addressBalanceMap_);
            if (isExtOnly_ || (cbCnt->fetch_add(1) >= 1)) {  // comparing with prev value
               updateAddrBalance_ = false;
            }
         } catch (std::exception& e) {
            if (logger_ != nullptr) {
               logger_->error("[hd::Leaf::getAddrBalance] Return data " \
                  "error - {}", e.what());
            }
         }
         if (!updateAddrBalance_) {
            invokeCb<std::vector<uint64_t>>(addressBalanceMap_, cbBal_, defVal);
         }
      };

      cbBal_[addr].push_back(cb);
      if (cbBal_.size() == 1) {
         btcWallet_->getAddrBalancesFromDB(cbAddrBalance);
         if (!isExtOnly_) {
            btcWalletInt_->getAddrBalancesFromDB(cbAddrBalance);
         }
      }
   } else {
      const auto itBal = addressBalanceMap_.find(addr.id());
      if (itBal == addressBalanceMap_.end()) {
         cb(defVal);
         return true;
      }
      cb(itBal->second);
   }
   return true;
}

bool hd::Leaf::getAddrTxN(const bs::Address &addr, std::function<void(uint32_t)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   if (updateAddrTxN_) {
      auto cbCnt = std::make_shared<std::atomic_uint>(0);
      const auto &cbTxN = [this, addr, cbCnt]
      (ReturnMessage<std::map<BinaryData, uint32_t>> txnMap) {
         try {
            const auto inTxnMap = txnMap.get();
            updateMap<std::map<BinaryData, uint32_t>>(inTxnMap, addressTxNMap_);
            if (isExtOnly_ || (cbCnt->fetch_add(1) >= 1)) {  // comparing with prev value
               updateAddrTxN_ = false;
            }
         } catch (const std::exception &e) {
            if (logger_ != nullptr) {
               logger_->error("[hd::Leaf::getAddrTxN] Return data error - {} ", \
                  "- Address {}", e.what(), addr.display());
            }
         }
         if (!updateAddrTxN_) {
            invokeCb<uint32_t>(addressTxNMap_, cbTxN_, 0);
         }
      };

      cbTxN_[addr].push_back(cb);
      if (cbTxN_.size() == 1) {
         btcWallet_->getAddrTxnCountsFromDB(cbTxN);
         if (!isExtOnly_) {
            btcWalletInt_->getAddrTxnCountsFromDB(cbTxN);
         }
      }
   } else {
      const auto itTxN = addressTxNMap_.find(addr.id());
      if (itTxN == addressTxNMap_.end()) {
         cb(0);
         return true;
      }
      cb(itTxN->second);
   }
   return true;
}

bool hd::Leaf::getActiveAddressCount(const std::function<void(size_t)> &cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   if (addressTxNMap_.empty() || updateAddrTxN_) {
      auto cbCnt = std::make_shared<std::atomic_uint>(0);
      const auto &cbTxN = [this, cb, cbCnt](ReturnMessage<std::map<BinaryData, uint32_t>> txnMap) {
         try {
            auto inTxnMap = txnMap.get();
            updateMap<std::map<BinaryData, uint32_t>>(inTxnMap, addressTxNMap_);
            if (isExtOnly_ || (cbCnt->fetch_add(1) == 1)) {  // comparing with prev value
               updateAddrTxN_ = false;
            }
         } catch (std::exception& e) {
            if (logger_ != nullptr) {
               logger_->error("[bs::Wallet::GetActiveAddressCount] Return data error - {} ", e.what());
            }
         }
         if (!updateAddrTxN_) {
            cb(addressTxNMap_.size());
         }
      };

      btcWallet_->getAddrTxnCountsFromDB(cbTxN);
      if (!isExtOnly_) {
         btcWalletInt_->getAddrTxnCountsFromDB(cbTxN);
      }
   } else {
      cb(addressTxNMap_.size());
   }
   return true;
}

BTCNumericTypes::balance_type hd::Leaf::getSpendableBalance() const
{
   return (Wallet::getSpendableBalance() - spendableBalanceCorrection_);
}

bool hd::Leaf::getSpendableTxOutList(std::function<void(std::vector<UTXO>)>cb
   , QObject *obj, uint64_t val)
{
   auto cbCnt = std::make_shared<std::atomic_uint>(0);
   auto result = std::make_shared<std::vector<UTXO>>();

   const auto &cbTxOutList = [this, cb, cbCnt, result](std::vector<UTXO> txOutList, uint32_t nbConf) {
      const auto curHeight = armory_ ? armory_->topBlock() : 0;
      for (const auto &utxo : txOutList) {
         const auto &addr = bs::Address::fromUTXO(utxo);
         const auto &path = getPathForAddress(addr);
         if (path.length() < 2) {
            continue;
         }
         if (utxo.getNumConfirm(curHeight) >= nbConf) {
            result->emplace_back(utxo);
         }
      }
      if (isExtOnly_ || cbCnt->fetch_add(1) > 0) {  // comparing with prev value
         cb(*result);
      }
   };
   const auto &cbTxOutListInt = [cbTxOutList](std::vector<UTXO> txOutList) {
      cbTxOutList(txOutList, kIntConfCount);
   };
   const auto &cbTxOutListExt = [cbTxOutList](std::vector<UTXO> txOutList) {
      cbTxOutList(txOutList, kExtConfCount);
   };
   bool rc = Wallet::getSpendableTxOutList(btcWallet_, cbTxOutListExt, obj, val);
   if (!isExtOnly_) {
      rc &= Wallet::getSpendableTxOutList(btcWalletInt_, cbTxOutListInt, obj, val);
   }
   return rc;
}

bool hd::Leaf::getSpendableZCList(std::function<void(std::vector<UTXO>)> cb
   , QObject *obj)
{
   auto cbCnt = std::make_shared<std::atomic_uint>(0);
   auto result = std::make_shared<std::vector<UTXO>>();
   const auto &cbWrap = [this, cb, cbCnt, result](std::vector<UTXO> utxos) {
      result->insert(result->end(), utxos.begin(), utxos.end());
      if (isExtOnly_ || (cbCnt->fetch_add(1) > 0)) {
         cb(*result);
      }
   };
   bool rc = Wallet::getSpendableZCList(btcWallet_, cbWrap, obj);
   if (!isExtOnly_) {
      rc &= Wallet::getSpendableZCList(btcWalletInt_, cbWrap, obj);
   }
   return rc;
}

bool hd::Leaf::getUTXOsToSpend(uint64_t val, std::function<void(std::vector<UTXO>)> cb) const
{
   auto cbCnt = std::make_shared<std::atomic_uint>(0);
   auto result = std::make_shared<std::vector<UTXO>>();
   const auto &cbWrap = [this, cb, cbCnt, result](std::vector<UTXO> utxos) {
      result->insert(result->end(), utxos.begin(), utxos.end());
      if (isExtOnly_ || (cbCnt->fetch_add(1) > 0)) {
         cb(*result);
      }
   };
   bool rc = Wallet::getUTXOsToSpend(btcWallet_, val, cbWrap);
   if (!isExtOnly_) {
      rc &= Wallet::getUTXOsToSpend(btcWalletInt_, val, cbWrap);
   }
   return rc;
}

bool hd::Leaf::getRBFTxOutList(std::function<void(std::vector<UTXO>)> cb) const
{
   auto cbCnt = std::make_shared<std::atomic_uint>(0);
   auto result = std::make_shared<std::vector<UTXO>>();
   const auto &cbWrap = [this, cb, cbCnt, result](std::vector<UTXO> utxos) {
      result->insert(result->end(), utxos.begin(), utxos.end());
      if (isExtOnly_ || (cbCnt->fetch_add(1) > 0)) {
         cb(*result);
      }
   };
   bool rc = Wallet::getRBFTxOutList(btcWallet_, cbWrap);
   if (!isExtOnly_) {
      rc &= Wallet::getRBFTxOutList(btcWalletInt_, cbWrap);
   }
   return rc;
}

bool hd::Leaf::getHistoryPage(uint32_t id, std::function<void(const Wallet *wallet
   , std::vector<ClientClasses::LedgerEntry>)> cb, bool onlyNew) const
{
   auto cbCnt = std::make_shared<std::atomic_uint>(0);
   auto result = std::make_shared<std::vector<ClientClasses::LedgerEntry>>();
   const auto &cbWrap = [this, cb, cbCnt, result](const Wallet *wallet
      , std::vector<ClientClasses::LedgerEntry> entries) {
      result->insert(result->end(), entries.begin(), entries.end());
      if (isExtOnly_ || (cbCnt->fetch_add(1) > 0)) {
         cb(wallet, *result);
      }
   };
   bool rc = Wallet::getHistoryPage(btcWallet_, id, cbWrap, onlyNew);
   if (!isExtOnly_) {
      rc &= Wallet::getHistoryPage(btcWalletInt_, id, cbWrap, onlyNew);
   }
   return rc;
}

std::string hd::Leaf::getAddressIndex(const bs::Address &addr)
{
   return getPathForAddress(addr).toString(false);
}

bool hd::Leaf::isExternalAddress(const bs::Address &addr) const
{
   const auto &path = getPathForAddress(addr);
   if (path.length() < 2) {
      return false;
   }
   return (path.get(-2) == addrTypeExternal);
}

bool hd::Leaf::addressIndexExists(const std::string &index) const
{
   const auto path = bs::hd::Path::fromString(index);
   if (path.length() < 2) {
      return false;
   }
   for (const auto &addr : addressMap_) {
      if (addr.first.path == path) {
         return true;
      }
   }
   return false;
}

bs::Address hd::Leaf::createAddressWithIndex(const std::string &index, AddressEntryType aet, bool signal)
{
   return createAddressWithPath({ bs::hd::Path::fromString(index), aet }, signal);
}

bs::Address hd::Leaf::createAddressWithPath(const AddrPoolKey &key, bool signal)
{
   if (key.path.length() < 2) {
      return {};
   }
   auto addrPath = key.path;
   if (key.path.length() > 2) {
      addrPath.clear();
      addrPath.append(key.path.get(-2));
      addrPath.append(key.path.get(-1));
   }
   for (const auto &addr : addressMap_) {
      if (addr.first.path == addrPath) {
         return addr.second;
      }
   }
   auto &lastIndex = (key.path.get(-2) == addrTypeInternal) ? lastIntIdx_ : lastExtIdx_;
   const auto addrIndex = key.path.get(-1);
   const int nbAddresses = addrIndex - lastIndex;
   if (nbAddresses > 0) {
      std::vector<std::pair<std::string, AddressEntryType>> request;
      for (bs::hd::Path::Elem i = lastIndex + 1; i < lastIndex + nbAddresses + 1; i++) {
         bs::hd::Path addrPath({ key.path.get(-2), i });
         request.push_back({ addrPath.toString(), key.aet });
      }
      const auto &cbNewAddrs = [this](const std::vector<std::pair<bs::Address, std::string>> &addrs) {
         for (const auto &addr : addrs) {
            addAddress(addr.first, addr.second, addr.first.getType(), false);
         }
      };
      newAddresses(request, cbNewAddrs);
      lastIndex += nbAddresses;
   }
   lastIndex++;
   return createAddress({ addrPath, key.aet }, [](const bs::Address &) {}, signal);
}

void hd::Leaf::onSaveToWallet(const std::vector<PooledAddress> &addresses)
{
   for (const auto &addr : addresses) {
      activeScanAddresses_.insert(addr.first);
   }
}

void hd::Leaf::onScanComplete()
{
   reset();
   const bool hasAddresses = !activeScanAddresses_.empty();
   if (hasAddresses) {
      std::vector<std::pair<std::string, AddressEntryType>> newAddrReq;
      newAddrReq.reserve(activeScanAddresses_.size());
      for (const auto &addr : activeScanAddresses_) {
         newAddrReq.push_back({ addr.path.toString(), addr.aet });
      }
      const auto &cbAddrsAdded = [this](const std::vector<std::pair<bs::Address, std::string>> &addrs) {
         for (const auto &addr : addrs) {
            addAddress(addr.first, addr.second, addr.first.getType(), false);
         }
         topUpAddressPool();
         registerWallet(armory_, true);
      };
      newAddresses(newAddrReq, cbAddrsAdded);
      activeScanAddresses_.clear();

      emit addressAdded();
      emit scanComplete(walletId());
      if (cbScanNotify_) {
         cbScanNotify_(index(), hasAddresses);
      }
   }
   else {
      emit scanComplete(walletId());
      if (cbScanNotify_) {
         cbScanNotify_(index(), hasAddresses);
      }
   }
}

bs::hd::Path::Elem hd::Leaf::getLastAddrPoolIndex(bs::hd::Path::Elem addrType) const
{
   bs::hd::Path::Elem result = 0;
   for (const auto &addr : addressPool_) {
      const auto &path = addr.first.path;
      if (path.get(-2) == addrType) {
         result = qMax(result, path.get(-1));
      }
   }
   if (!result) {
      result = (addrType == addrTypeInternal) ? lastIntIdx_ : lastExtIdx_;
   }
   return result;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , SignContainer *container, const std::shared_ptr<spdlog::logger> &logger)
   : Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::Authentication, true)
{
   intAddressPoolSize_ = 0;
   extAddressPoolSize_ = 5;
   poolAET_ = { AddressEntryType_P2WPKH };
}

bs::Address hd::AuthLeaf::createAddress(const AddrPoolKey &key, const CbAddress &cb, bool signal)
{
   if (userId_.isNull()) {
      tempAddresses_.insert(key);
      return {};
   }
   return hd::Leaf::createAddress(key, cb, signal);
}

void hd::AuthLeaf::topUpAddressPool(const std::function<void()> &cb, size_t intAddresses, size_t extAddresses)
{
   if (userId_.isNull()) {
      return;
   }
   hd::Leaf::topUpAddressPool(cb, intAddresses, extAddresses);
}

void hd::AuthLeaf::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   if (userId.isNull()) {
      reset();
      return;
   }

   for (const auto &addr : tempAddresses_) {
      createAddress(addr, nullptr, false);
      lastExtIdx_ = std::max(lastExtIdx_, addr.path.get(-1) + 1);
   }
   topUpAddressPool();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::CCLeaf::CCLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , SignContainer *container, const std::shared_ptr<spdlog::logger> &logger
   , bool extOnlyAddresses)
   : hd::Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::ColorCoin, extOnlyAddresses)
   , validationStarted_(false), validationEnded_(false)
{}

hd::CCLeaf::~CCLeaf()
{
   validationStarted_ = false;
}

void hd::CCLeaf::setData(const std::string &data)
{
   checker_ = std::make_shared<TxAddressChecker>(bs::Address(data), armory_);
}

void hd::CCLeaf::setArmory(const std::shared_ptr<ArmoryObject> &armory)
{
   hd::Leaf::setArmory(armory);
   if (armory_) {
      connect(armory_.get(), SIGNAL(stateChanged(ArmoryConnection::State)), this
         , SLOT(onStateChanged(ArmoryConnection::State)), Qt::QueuedConnection);
   }
   if (checker_ && armory) {
      checker_->setArmory(armory);
   }
   if (checker_ && !validationStarted_) {
      validationEnded_ = false;
      validationProc();
   }
}

void hd::CCLeaf::refreshInvalidUTXOs(const bool& ZConly)
{
   {
      QMutexLocker lock(&addrMapsMtx_);
      updateAddrBalance_ = false;
      addressBalanceMap_.clear();
   }

   if (!ZConly) {
      const auto &cbRefresh = [this](std::vector<UTXO> utxos) {
         const auto &cbUpdateSpendableBalance = [this](const std::vector<UTXO> &spendableUTXOs) {
            QMutexLocker lock(&addrMapsMtx_);
            for (const auto &utxo : spendableUTXOs) {
               const auto &addr = utxo.getRecipientScrAddr();
               auto &balanceVec = addressBalanceMap_[addr];
               if (balanceVec.empty()) {
                  balanceVec = { 0, 0, 0 };
               }
               balanceVec[0] += utxo.getValue();
               balanceVec[1] += utxo.getValue();
            }
         };
         findInvalidUTXOs(utxos, cbUpdateSpendableBalance);
      };
      hd::Leaf::getSpendableTxOutList(cbRefresh, this);
   }

   const auto &cbRefreshZC = [this](std::vector<UTXO> utxos) {
      const auto &cbUpdateZcBalance = [this](const std::vector<UTXO> &ZcUTXOs) {
         QMutexLocker lock(&addrMapsMtx_);
         for (const auto &utxo : ZcUTXOs) {
            auto &balanceVec = addressBalanceMap_[utxo.getRecipientScrAddr()];
            if (balanceVec.empty()) {
               balanceVec = { 0, 0, 0 };
            }
            balanceVec[2] += utxo.getValue();
            balanceVec[0] = balanceVec[1] + balanceVec[2];
         }
      };
      findInvalidUTXOs(utxos, cbUpdateZcBalance);
   };
   hd::Leaf::getSpendableZCList(cbRefreshZC, this);
}

void hd::CCLeaf::validationProc()
{
   validationStarted_ = true;
   if (!armory_ || (armory_->state() != ArmoryConnection::State::Ready)) {
      validationStarted_ = false;
      return;
   }
   validationEnded_ = true;
   refreshInvalidUTXOs();
   hd::Leaf::firstInit();

   if (!validationStarted_) {
      return;
   }

   auto addressesToCheck = std::make_shared<std::map<bs::Address, int>>();

   for (const auto &addr : getUsedAddressList()) {
      addressesToCheck->emplace(addr, -1);
   }

   for (const auto &addr : getUsedAddressList()) {
      const auto &cbLedger = [this, addr, addressesToCheck]
                              (const std::shared_ptr<AsyncClient::LedgerDelegate> &ledger) {
         if (!validationStarted_) {
            return;
         }
         const auto &cbCheck = [this, addr, addressesToCheck](const Tx &tx) {
            const auto &cbResult = [this, tx](bool contained) {
               if (!contained && tx.isInitialized()) {
                  invalidTxHash_.insert(tx.getThisHash());
               }
            };
            checker_->containsInputAddress(tx, cbResult, lotSizeInSatoshis_);

            auto it = addressesToCheck->find(addr);
            if (it != addressesToCheck->end()) {
               it->second = it->second - 1;
               if (it->second <= 0) {
                  addressesToCheck->erase(it);
               }
            }

            bool empty = true;
            for (const auto& it : *addressesToCheck) {
               if (it.second > 0) {
                  empty = false;
                  break;
               }
            }

            if (empty) {
               emit walletReset();
            }
         };

         const auto &cbHistory = [this, cbCheck, addr, addressesToCheck]
         (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
            try {
               const auto &le = entries.get();
               (*addressesToCheck)[addr] = le.size();

               for (const auto &entry : le) {
                  armory_->getTxByHash(entry.getTxHash(), cbCheck);
               }
            } catch (const std::exception &e) {
               if (logger_ != nullptr) {
                  logger_->error("[hd::CCLeaf::validationProc] Return data " \
                     "error - {}", e.what());
               }
            }
         };
         const auto &cbPages = [this,  cbHistory, ledger] (ReturnMessage<uint64_t> pages) {
            try {
               const auto pageCnt = pages.get();
               for (uint32_t pageId = 0; pageId < pageCnt; ++pageId) {
                  ledger->getHistoryPage(pageId, cbHistory);
               }
            }
            catch (const std::exception &e) {
               if (logger_) {
                  logger_->error("[hd::CCLeaf::validationProc] cbPages failed: {}", e.what());
               }
            }
         };
         ledger->getPageCount(cbPages);
      };
      getLedgerDelegateForAddress(addr, cbLedger, this);
   }
}

void hd::CCLeaf::findInvalidUTXOs(const std::vector<UTXO> &utxos, std::function<void(const std::vector<UTXO> &)> cb)
{
   std::set<BinaryData> txHashes;
   std::map<BinaryData, UTXO> utxoMap;
   for (const auto &utxo : utxos) {
      if (!validationStarted_) {
         return;
      }
      const auto &hash = utxo.getTxHash();
      txHashes.insert(hash);
      utxoMap[hash] = utxo;
   }
   const auto &cbProcess = [this, utxoMap, cb, utxos](const std::vector<Tx> &txs) {
      struct TxResultData {
         Tx    tx;
         UTXO  utxo;
      };
      struct Result {
         uint64_t invalidBalance = 0;
         std::map<BinaryData, TxResultData> txHashMap;
      };
      auto result = std::make_shared<Result>();

      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         const auto &itUtxo = utxoMap.find(txHash);
         if (itUtxo == utxoMap.end()) {
            continue;
         }
         result->txHashMap[txHash] = { tx, itUtxo->second };
      }

      const auto txHashMap = result->txHashMap;
      for (const auto &txPair : txHashMap) {
         const auto &txHash = txPair.first;
         const auto &txData = txPair.second;
         const auto &cbResult = [this, txHash, txData, result, cb, utxos](bool contained) {
            if (!contained) {
               invalidTx_.insert(txData.utxo);
               invalidTxHash_.insert(txHash);
               result->invalidBalance += txData.utxo.getValue();
            }
            result->txHashMap.erase(txHash);
            if (result->txHashMap.empty()) {
               balanceCorrection_ += result->invalidBalance / BTCNumericTypes::BalanceDivider;
               cb(filterUTXOs(utxos));
            }
         };
         checker_->containsInputAddress(txData.tx, cbResult, lotSizeInSatoshis_, txData.utxo.getValue());
      }
   };
   if (txHashes.empty()) {
      cb(utxos);
   }
   else {
      armory_->getTXsByHash(txHashes, cbProcess);
   }
}

void hd::CCLeaf::firstInit(bool force)
{
   if (force) {
      validationStarted_ = false;
   }
   if (checker_ && !validationStarted_) {
      validationEnded_ = false;
      validationProc();
   }
}

void hd::CCLeaf::onStateChanged(ArmoryConnection::State state)
{
   if (state == ArmoryConnection::State::Ready) {
      firstInit(true);
   }
}

void hd::CCLeaf::onZeroConfReceived(const std::vector<bs::TXEntry> entries)
{
   hd::Leaf::onZeroConfReceived(entries);
   refreshInvalidUTXOs(true);
}

std::vector<UTXO> hd::CCLeaf::filterUTXOs(const std::vector<UTXO> &utxos) const
{
   std::vector<UTXO> result;
   for (const auto &txOut : utxos) {
      if (invalidTx_.find(txOut) == invalidTx_.end()) {
         result.emplace_back(txOut);
      }
   }
   return result;
}

bool hd::CCLeaf::getSpendableTxOutList(std::function<void(std::vector<UTXO>)>cb
   , QObject *obj, uint64_t val)
{
   if (validationStarted_ && !validationEnded_) {
      return false;
   }
   const auto &cbTxOutList = [this, cb](std::vector<UTXO> txOutList) {
      cb(filterUTXOs(txOutList));
   };
   return hd::Leaf::getSpendableTxOutList(cbTxOutList, obj, val);
}

bool hd::CCLeaf::getSpendableZCList(std::function<void(std::vector<UTXO>)> cb
   , QObject *obj)
{
   if (validationStarted_ && !validationEnded_) {
      return false;
   }
   const auto &cbZCList = [this, cb](std::vector<UTXO> txOutList) {
      cb(filterUTXOs(txOutList));
   };
   return hd::Leaf::getSpendableZCList(cbZCList, obj);
}

bool hd::CCLeaf::isBalanceAvailable() const
{
   return (validationEnded_ || !checker_) ? hd::Leaf::isBalanceAvailable() : false;
}

BTCNumericTypes::balance_type hd::CCLeaf::correctBalance(BTCNumericTypes::balance_type balance, bool apply) const
{
   if (!lotSizeInSatoshis_) {
      return 0;
   }
   const BTCNumericTypes::balance_type correction = apply ? balanceCorrection_ : 0;
   return (balance - correction) * BTCNumericTypes::BalanceDivider / lotSizeInSatoshis_;
}

BTCNumericTypes::balance_type hd::CCLeaf::getSpendableBalance() const
{
   return correctBalance(hd::Leaf::getSpendableBalance());
}

BTCNumericTypes::balance_type hd::CCLeaf::getUnconfirmedBalance() const
{
   return correctBalance(hd::Leaf::getUnconfirmedBalance(), false);
}

BTCNumericTypes::balance_type hd::CCLeaf::getTotalBalance() const
{
   return correctBalance(hd::Leaf::getTotalBalance());
}

bool hd::CCLeaf::getAddrBalance(const bs::Address &addr, std::function<void(std::vector<uint64_t>)> cb) const
{
   if (!lotSizeInSatoshis_ || !validationEnded_ || !Wallet::isBalanceAvailable()) {
      return false;
   }
   std::vector<uint64_t> xbtBalances;
   {
      const auto itBal = addressBalanceMap_.find(addr.prefixed());
      if (itBal == addressBalanceMap_.end()) {
         cb({0,0,0});
         return true;
      }
      xbtBalances = itBal->second;
   }
   for (auto &balance : xbtBalances) {
      balance /= lotSizeInSatoshis_;
   }
   cb(xbtBalances);
   return true;
}

bool hd::CCLeaf::isTxValid(const BinaryData &txHash) const
{
   return (invalidTxHash_.find(txHash) == invalidTxHash_.end());
}

BTCNumericTypes::balance_type hd::CCLeaf::getTxBalance(int64_t val) const
{
   if (!lotSizeInSatoshis_) {
      return 0;
   }
   return (double)val / lotSizeInSatoshis_;
}

QString hd::CCLeaf::displayTxValue(int64_t val) const
{
   return QLocale().toString(getTxBalance(val), 'f', 0);
}

QString hd::CCLeaf::displaySymbol() const
{
   return suffix_.empty() ? hd::Leaf::displaySymbol() : QString::fromStdString(suffix_);
}
