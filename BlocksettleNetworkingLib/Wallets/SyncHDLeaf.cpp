#include "SyncHDLeaf.h"

#include <unordered_map>
#include <QLocale>
#include <QMutexLocker>
#include "CheckRecipSigner.h"
#include "SignContainer.h"

const uint32_t kExtConfCount = 6;
const uint32_t kIntConfCount = 1;

using namespace bs::sync;


hd::BlockchainScanner::BlockchainScanner(const std::string &walletId, const cb_save_to_wallet &cbSave
   , const cb_completed &cbComplete)
   : walletId_(walletId), rescanWalletId_("rescan_" + walletId)
   , cbSaveToWallet_(cbSave), cbCompleted_(cbComplete), processing_(-1)
{}

std::vector<hd::BlockchainScanner::PooledAddress> hd::BlockchainScanner::newAddresses(
   const std::vector<AddrPoolKey> &)
{
   return {};  //stub
}

std::vector<hd::BlockchainScanner::PooledAddress> hd::BlockchainScanner::generateAddresses(
   bs::hd::Path::Elem prefix, bs::hd::Path::Elem start, size_t nb, AddressEntryType aet)
{
   std::vector<AddrPoolKey> request;
   request.reserve(nb);
   for (bs::hd::Path::Elem i = start; i < start + nb; i++) {
      bs::hd::Path addrPath({ prefix, i });
      request.push_back({ addrPath, aet });
   }
   return newAddresses(request);
}

std::vector<BinaryData> hd::BlockchainScanner::getRegAddresses(const std::vector<hd::BlockchainScanner::PooledAddress> &src)
{
   std::vector<BinaryData> result;
   for (const auto &addr : src) {
      result.push_back(addr.second.prefixed());
   }
   return result;
}

void hd::BlockchainScanner::fillPortion(bs::hd::Path::Elem start, unsigned int size)
{
   currentPortion_.activeAddresses.clear();
   currentPortion_.addresses.clear();
   currentPortion_.addresses.reserve(size * 4);
   currentPortion_.poolKeyByAddr.clear();
   currentPortion_.start = start;
   currentPortion_.end = start + size - 1;

   for (bs::hd::Path::Elem addrType : {0, 1}) {
      for (const auto aet : { AddressEntryType_P2SH, AddressEntryType_P2WPKH }) {
         const auto addrs = generateAddresses(addrType, start, size, aet);
         currentPortion_.addresses.insert(currentPortion_.addresses.end(), addrs.begin(), addrs.end());
         for (const auto &addr : addrs) {
            currentPortion_.poolKeyByAddr[addr.second] = addr.first;
         }
      }
   }
}

void hd::BlockchainScanner::scanAddresses(unsigned int startIdx, unsigned int portionSize, const cb_write_last &cbw)
{
   if (startIdx == UINT32_MAX) {    // special index to skip rescan for particular wallet
      if (cbCompleted_) {
         cbCompleted_();
      }
      return;
   }
   portionSize_ = portionSize;
   cbWriteLast_ = cbw;
   fillPortion(startIdx, portionSize);
   if (cbWriteLast_) {
      cbWriteLast_(walletId_, startIdx);
   }
   currentPortion_.registered = true;

   rescanRegId_ = armoryConn_->registerWallet(rescanWallet_, rescanWalletId_
      , getRegAddresses(currentPortion_.addresses), nullptr, true);
}

void hd::BlockchainScanner::onRefresh(const std::vector<BinaryData> &ids)
{
   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }
   const auto &it = std::find(ids.begin(), ids.end(), rescanRegId_);
   if (it == ids.end()) {
      return;
   }
   processPortion();
}

void hd::BlockchainScanner::processPortion()
{
   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }

   processing_ = (int)currentPortion_.start;
   currentPortion_.registered = false;

   const auto &cbProcess = [this] {
      if (!currentPortion_.activeAddresses.empty()) {
         if (cbSaveToWallet_) {
            std::sort(currentPortion_.activeAddresses.begin(), currentPortion_.activeAddresses.end()
               , [](const PooledAddress &a, const PooledAddress &b)
            { return (a.first.path < b.first.path) && (a.first.aet < b.first.aet); });
            cbSaveToWallet_(currentPortion_.activeAddresses);
         }
         if (cbWriteLast_) {
            cbWriteLast_(walletId_, currentPortion_.end + 1);
         }
         fillPortion(currentPortion_.end + 1, portionSize_);
         currentPortion_.registered = true;
         rescanRegId_ = armoryConn_->registerWallet(rescanWallet_
            , rescanWalletId_, getRegAddresses(currentPortion_.addresses), {});
      }
      else {
         currentPortion_.start = currentPortion_.end = 0;
         currentPortion_.addresses.clear();
         processing_ = -1;
         rescanWallet_.reset();

         if (cbWriteLast_) {
            cbWriteLast_(walletId_, UINT32_MAX);
         }
         if (cbCompleted_) {
            cbCompleted_();
         }
      }
   };

   const auto &cbTXs = [this, cbProcess](std::vector<Tx> txs) {
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

      const auto &cbInputs = [this, opTxIndices, cbProcess](std::vector<Tx> inputs) {
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
      armoryConn_->getTXsByHash(opTxHashes, cbInputs);
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
         armoryConn_->getTXsByHash(txHashes, cbTXs);
      }
   };
   armoryConn_->getWalletsHistory({ rescanWalletId_ }, cbHistory);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::Leaf::Leaf(const std::string &walletId, const std::string &name, const std::string &desc
   ,const std::shared_ptr<SignContainer> &container, const std::shared_ptr<spdlog::logger> &logger
   , bs::core::wallet::Type type, bool extOnlyAddresses)
   : bs::sync::Wallet(container, logger)
   , hd::BlockchainScanner(walletId
      , [this](const std::vector<PooledAddress> &addrs) { onSaveToWallet(addrs); }
      , [this] { onScanComplete(); })
   , walletId_(walletId), type_(type), name_(name), desc_(desc), isExtOnly_(extOnlyAddresses)
{ }

hd::Leaf::~Leaf() = default;

void hd::Leaf::synchronize()
{
   const auto &cbProcess = [this](bs::sync::WalletData data) {
      isWatchingOnly_ = data.isWatchingOnly;
      encryptionTypes_ = data.encryptionTypes;
      encryptionKeys_ = data.encryptionKeys;
      encryptionRank_ = data.encryptionRank;
      netType_ = data.netType;
      //stub
      emit synchronized();
   };
   signContainer_->syncWallet(walletId(), cbProcess);
}

void hd::Leaf::setArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   bs::sync::Wallet::setArmory(armory);
   hd::BlockchainScanner::setArmory(armory);
   if (armory_) {
      connect(armory_.get(), &ArmoryConnection::zeroConfReceived, this, &hd::Leaf::onZeroConfReceived, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::refresh, this, &hd::Leaf::onRefresh, Qt::QueuedConnection);
   }
}

void hd::Leaf::init(const bs::hd::Path &path)
{
   if (path != path_) {
      path_ = path;
      suffix_.clear();
      const auto idx = index();
      for (size_t i = 4; i > 0; i--) {
         unsigned char c = (idx >> (8 * (i - 1))) & 0xff;
         if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'))) {
            suffix_.append(1, c);
         }
      }
      if (suffix_.empty()) {
         suffix_ = std::to_string(idx);
      }
      walletName_ = name_ + "/" + suffix_;
   }

   if (!path.length()) {
      reset();
   }
}

void hd::Leaf::onZeroConfReceived(const std::vector<bs::TXEntry>)
{
//!   activateAddressesFromLedger(armory_->getZCentries(reqId));
}  // if ZC is received, then likely wallet already contains the participating address

void hd::Leaf::onRefresh(std::vector<BinaryData> ids)
{
   hd::BlockchainScanner::onRefresh(ids);
}

void hd::Leaf::firstInit(bool force)
{
   bs::sync::Wallet::firstInit();

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
   const auto &cb = [this](std::vector<Tx> txs) {
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
      const auto &cbInputs = [this, activated, opTxIndices](std::vector<Tx> inputs) {
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

/*bool hd::Leaf::copyTo(std::shared_ptr<hd::Leaf> &leaf) const
{
   for (const auto &addr : addressMap_) {
      const auto &address = addr.second;
      const auto newAddr = leaf->createAddress(addr.first, false);
      const auto comment = getAddressComment(address);
      if (!comment.empty()) {
         if (!leaf->setAddressComment(newAddr, comment)) {
         }
      }
   }
   for (const auto &addr : tempAddresses_) {
      leaf->createAddress(addr, false);
   }
   leaf->path_ = path_;
   leaf->lastExtIdx_ = lastExtIdx_;
   leaf->lastIntIdx_ = lastIntIdx_;
   return true;
}*/

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
bs::Address hd::Leaf::getNewExtAddress(AddressEntryType aet)
{
   return createAddress(aet, false);
}

// Return an internal-facing address.
bs::Address hd::Leaf::getNewIntAddress(AddressEntryType aet)
{
   if (isExtOnly_) {
      return {};
   }
   return createAddress(aet, true);
}

// Return a change address.
bs::Address hd::Leaf::getNewChangeAddress(AddressEntryType aet)
{
   return createAddress(aet, isExtOnly_ ? false : true);
}

bs::Address hd::Leaf::getRandomChangeAddress(AddressEntryType aet)
{
   if (isExtOnly_) {
      if (extAddresses_.empty()) {
         return getNewExtAddress(aet);
      } else if (extAddresses_.size() == 1) {
         return extAddresses_[0];
      }
      return extAddresses_[qrand() % extAddresses_.size()];
   }
   else {
      if (!lastIntIdx_) {
         return getNewChangeAddress(aet);
      }
      else {
         return intAddresses_[qrand() % intAddresses_.size()];
      }
   }
}

/*AddressEntryType hd::Leaf::getAddrTypeForAddr(const BinaryData &addr)
{
   const auto addrEntry = getAddressEntryForAddr(addr);
   if (addrEntry == nullptr) {
      return AddressEntryType_Default;
   }
   return addrEntry->getType();
}*/

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

std::vector<std::string> hd::Leaf::registerWallet(const std::shared_ptr<ArmoryConnection> &armory, bool asNew)
{
   setArmory(armory);

   if (armory_) {
      const auto addrsExt = getAddrHashesExt();
      std::vector<std::string> regIds;;
      const auto regIdSet = std::make_shared<std::set<std::string>>();

      const auto &cbRegisterExt = [this, regIdSet](const std::string &regId) {
         btcWallet_->setUnconfirmedTarget(kExtConfCount);
         regIdSet->erase(regId);
         if (isExtOnly_ || regIdSet->empty()) {
            emit walletReady(QString::fromStdString(walletId()));
         }
      };
      const auto &cbRegisterInt = [this, regIdSet](const std::string &regId) {
         btcWalletInt_->setUnconfirmedTarget(kIntConfCount);
         regIdSet->erase(regId);
         if (regIdSet->empty()) {
            emit walletReady(QString::fromStdString(walletId()));
         }
      };

      const auto regIdExt = armory_->registerWallet(btcWallet_, walletId()
         , addrsExt, cbRegisterExt, asNew);
      regIdSet->insert(regIdExt);
      regIds.push_back(regIdExt);

      if (!isExtOnly_) {
         const auto addrsInt = getAddrHashesInt();
         const auto regIdInt = armory_->registerWallet(btcWalletInt_
            , getWalletIdInt(), addrsInt, cbRegisterInt, asNew);
         regIdSet->insert(regIdInt);
         regIds.push_back(regIdInt);
      }
      return regIds;
   }
   return {};
}

void hd::Leaf::unregisterWallet()
{
   addrPrefixedHashes_.clear();
   addressPool_.clear();
   Wallet::unregisterWallet();
   btcWalletInt_.reset();
}

bs::Address hd::Leaf::createAddress(AddressEntryType aet, bool isInternal)
{
   topUpAddressPool();
   bs::hd::Path addrPath;
   if (isInternal) {
      if (isExtOnly_) {
         return {};
      }
      addrPath.append(addrTypeInternal);
      addrPath.append(lastIntIdx_++);
   }
   else {
      addrPath.append(addrTypeExternal);
      addrPath.append(lastExtIdx_++);
   }
   return createAddress({ addrPath, aet });
}

bs::Address hd::Leaf::createAddress(const AddrPoolKey &key, bool signal)
{
   const bool isInternal = (key.path.get(-2) == addrTypeInternal);
   if (isInternal && isExtOnly_) {
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
      const auto newAddrs = newAddresses({ keyCopy });
      if (newAddrs.empty()) {
         return {};
      }
      result = newAddrs[0].second;
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

void hd::Leaf::topUpAddressPool(size_t nbIntAddresses, size_t nbExtAddresses)
{
   const size_t nbPoolInt = nbIntAddresses ? 0 : getLastAddrPoolIndex(addrTypeInternal) - lastIntIdx_ + 1;
   const size_t nbPoolExt = nbExtAddresses ? 0 : getLastAddrPoolIndex(addrTypeExternal) - lastExtIdx_ + 1;
   nbIntAddresses = qMax(nbIntAddresses, intAddressPoolSize_);
   nbExtAddresses = qMax(nbExtAddresses, extAddressPoolSize_);

   for (const auto aet : poolAET_) {
      if (nbPoolInt < (intAddressPoolSize_ / 4)) {
         const auto intAddresses = generateAddresses(addrTypeInternal, lastIntIdx_, nbIntAddresses, aet);
         for (const auto &addr : intAddresses) {
            addressPool_[addr.first] = addr.second;
            poolByAddr_[addr.second] = addr.first;
         }
      }

      if (nbPoolExt < (extAddressPoolSize_ / 4)) {
         const auto extAddresses = generateAddresses(addrTypeExternal, lastExtIdx_, nbExtAddresses, aet);
         for (const auto &addr : extAddresses) {
            addressPool_[addr.first] = addr.second;
            poolByAddr_[addr.second] = addr.first;
         }
      }
   }
}

/*std::shared_ptr<AddressEntry> hd::Leaf::getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
   , AddressEntryType ae_type)
{
   if (ae_type == AddressEntryType_Default) {
      ae_type = defaultAET_;
   }

   std::shared_ptr<AddressEntry> aePtr = nullptr;
   switch (ae_type)
   {
   case AddressEntryType_P2PKH:
      aePtr = std::make_shared<AddressEntry_P2PKH>(assetPtr, true);
      break;

   case AddressEntryType_P2SH:
      {
         const auto nested = std::make_shared<AddressEntry_P2WPKH>(assetPtr);
         aePtr = std::make_shared<AddressEntry_P2SH>(nested);
      }
      break;

   case AddressEntryType_P2WPKH:
      aePtr = std::make_shared<AddressEntry_P2WPKH>(assetPtr);
      break;

   default:
      throw WalletException("unsupported address entry type");
   }

   return aePtr;
}*/

hd::BlockchainScanner::AddrPoolKey hd::Leaf::getAddressIndexForAddr(const BinaryData &addr) const
{
   bs::Address p2pk(addr, AddressEntryType_P2PKH);
   bs::Address p2sh(addr, AddressEntryType_P2SH);
   hd::BlockchainScanner::AddrPoolKey index;
   for (const auto &bd : { p2pk.unprefixed(), p2sh.unprefixed() }) {
      const auto itIndex = addrToIndex_.find(bd);
      if (itIndex != addrToIndex_.end()) {
         index = itIndex->second;
         break;
      }
   }
   return index;
}

hd::BlockchainScanner::AddrPoolKey hd::Leaf::getAddressIndex(const bs::Address &addr) const
{
   auto itIndex = addrToIndex_.find(addr.prefixed());
   if (itIndex == addrToIndex_.end()) {
      itIndex = addrToIndex_.find(addr.unprefixed());
      if (itIndex == addrToIndex_.end()) {
         return {};
      }
   }
   return itIndex->second;
}

bs::hd::Path hd::Leaf::getPathForAddress(const bs::Address &addr) const
{
   const auto index = getAddressIndex(addr);
   if (index.empty()) {
      return {};
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

int hd::Leaf::addAddress(const bs::Address &addr, const std::string &index, AddressEntryType aet)
{
   const auto path = bs::hd::Path::fromString(index);
   const bool isInternal = (path.get(-2) == addrTypeInternal);
   const int id = bs::sync::Wallet::addAddress(addr, index, aet);
   if (isInternal) {
      intAddresses_.push_back(addr);
      addrPrefixedHashes_.internal.insert(addr.id());
   } else {
      extAddresses_.push_back(addr);
      addrPrefixedHashes_.external.insert(addr.id());
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
                  "- Address {}", e.what(), addr.display().toStdString());
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
   const auto &cbTxOutListInt = [this, cbTxOutList](std::vector<UTXO> txOutList) {
      cbTxOutList(txOutList, kIntConfCount);
   };
   const auto &cbTxOutListExt = [this, cbTxOutList](std::vector<UTXO> txOutList) {
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
      for (const auto &addr : generateAddresses(key.path.get(-2), lastIndex, nbAddresses, key.aet)) {
         lastIndex++;
         createAddress(addr.first, signal);
      }
   }
   lastIndex++;
   return createAddress({ addrPath, key.aet }, signal);
}

void hd::Leaf::onSaveToWallet(const std::vector<PooledAddress> &addresses)
{
   for (const auto &addr : addresses) {
      createAddressWithPath(addr.first, false);
   }
   if (!addresses.empty()) {
      emit addressAdded();
   }
}

void hd::Leaf::onScanComplete()
{
   const bool hasAddresses = (getUsedAddressCount() > 0);
   if (hasAddresses) {
      topUpAddressPool();
      const auto &regId = registerWallet(armory_, true);
   }
   emit scanComplete(walletId());
   if (cbScanNotify_) {
      cbScanNotify_(index(), hasAddresses);
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
      result = (addrType == addrTypeInternal) ? lastIntIdx_ - 1 : lastExtIdx_ - 1;
   }
   return result;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<spdlog::logger> &logger)
   : Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::Authentication)
{
   intAddressPoolSize_ = 0;
   extAddressPoolSize_ = 0;
}

bs::Address hd::AuthLeaf::createAddress(const AddrPoolKey &key, bool signal)
{
   if (userId_.isNull()) {
      tempAddresses_.insert(key);
      return {};
   }
   return hd::Leaf::createAddress(key, signal);
}

void hd::AuthLeaf::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   if (userId.isNull()) {
      reset();
      return;
   }

   for (const auto &addr : tempAddresses_) {
      createAddress(addr, false);
      lastExtIdx_ = std::max(lastExtIdx_, addr.path.get(-1) + 1);
   }
   const auto poolAddresses = generateAddresses(addrTypeExternal, lastExtIdx_, 5, AddressEntryType_P2WPKH);
   for (const auto &addr : poolAddresses) {
      addressPool_[addr.first] = addr.second;
      poolByAddr_[addr.second] = addr.first;
   }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::CCLeaf::CCLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<spdlog::logger> &logger
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

void hd::CCLeaf::setArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   hd::Leaf::setArmory(armory);
   if (armory_) {
      connect(armory_.get(), SIGNAL(stateChanged(ArmoryConnection::State)), this, SLOT(onStateChanged(ArmoryConnection::State)), Qt::QueuedConnection);
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
         const auto &cbCheck = [this, addr, addressesToCheck](Tx tx) {
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
   const auto &cbProcess = [this, utxoMap, cb, utxos](std::vector<Tx> txs) {
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
