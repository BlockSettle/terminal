#include "SyncHDLeaf.h"

#include <unordered_map>
#include <QLocale>
#include <QMutexLocker>
#include "CheckRecipSigner.h"
#include "WalletSignerContainer.h"

const uint32_t kExtConfCount = 6;
const uint32_t kIntConfCount = 1;

using namespace bs::sync;


hd::Leaf::Leaf(const std::string &walletId, const std::string &name, const std::string &desc
   , WalletSignerContainer *container, const std::shared_ptr<spdlog::logger> &logger
   , bs::core::wallet::Type type, bool extOnlyAddresses)
   : bs::sync::Wallet(container, logger)
   , walletId_(walletId)
   , type_(type)
   , name_(name)
   , desc_(desc)
   , isExtOnly_(extOnlyAddresses)
{}

hd::Leaf::~Leaf() = default;

void hd::Leaf::synchronize(const std::function<void()> &cbDone)
{
   reset();

   const auto &cbProcess = [this, cbDone](bs::sync::WalletData data)
   {
      encryptionTypes_ = data.encryptionTypes;
      encryptionKeys_ = data.encryptionKeys;
      encryptionRank_ = data.encryptionRank;
      netType_ = data.netType;
      if (wct_) {
         wct_->metadataChanged(walletId());
      }

      if (data.highestExtIndex == UINT32_MAX ||
         data.highestIntIndex == UINT32_MAX)
         throw WalletException("unintialized addr chain use index");

      lastExtIdx_ = data.highestExtIndex;
      lastIntIdx_ = data.highestIntIndex;

      logger_->debug("[sync::hd::Leaf::synchronize] {}: last indices {}+{}={} address[es]"
         , walletId(), lastExtIdx_, lastIntIdx_, data.addresses.size());
      for (const auto &addr : data.addresses) {
         addAddress(addr.address, addr.index, addr.address.getType(), false);
         setAddressComment(addr.address, addr.comment, false);
      }

      for (const auto &addr : data.addrPool) {
         //addPool normally won't contain comments
         const auto path = bs::hd::Path::fromString(addr.index);
         addressPool_[{ path, addr.address.getType() }] = addr.address;
         poolByAddr_[addr.address] = { path, addr.address.getType() };
      }

      for (const auto &txComment : data.txComments) {
         setTransactionComment(txComment.txHash, txComment.comment, false);
      }

      if (cbDone)
         cbDone();
   };

   signContainer_->syncWallet(walletId(), cbProcess);
}

void hd::Leaf::setPath(const bs::hd::Path &path)
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

bool hd::Leaf::isOwnId(const std::string &wId) const
{
   if (wId == walletId()) {
      return true;
   }
   if (!isExtOnly_ && (walletIdInt() == wId)) {
      return true;
   }
   return false;
}

void hd::Leaf::onRefresh(const std::vector<BinaryData> &ids, bool online)
{
   const auto &cbRegisterExt = [this, online] {
      if (isExtOnly_ || (regIdExt_.empty() && regIdInt_.empty())) {
         if (online) {
            postOnline();
         }
      }
   };
   const auto &cbRegisterInt = [this, online] {
      if (regIdExt_.empty() && regIdInt_.empty()) {
         if (online) {
            postOnline();
         }
      }
   };

   {
      std::unique_lock<std::mutex> lock(regMutex_);
      if (!regIdExt_.empty() || !regIdInt_.empty()) {
         for (const auto &id : ids) {
            if (id.isNull()) {
               continue;
            }
            logger_->debug("[{}] {}: id={}, extId={}, intId={}", __func__, walletId()
               , id.toBinStr(), regIdExt_, regIdInt_);
            if (id == regIdExt_) {
               regIdExt_.clear();
               cbRegisterExt();
            } else if (id == regIdInt_) {
               regIdInt_.clear();
               cbRegisterInt();
            }
         }
      }
   }

   if (!unconfTgtRegIds_.empty()) {
      for (const auto &id : ids) {
         const auto it = std::find(unconfTgtRegIds_.cbegin(), unconfTgtRegIds_.cend(), id.toBinStr());
         if (it != unconfTgtRegIds_.end()) {
            unconfTgtRegIds_.erase(it);
         }
      }

      if (unconfTgtRegIds_.empty()) {
         bs::sync::Wallet::init();
      }
   }
}

std::vector<std::string> hd::Leaf::setUnconfirmedTarget()
{
   std::vector<std::string> regIDs;

   if (btcWallet_) {
      regIDs.push_back(btcWallet_->setUnconfirmedTarget(kExtConfCount));
   }
   if (btcWalletInt_) {
      regIDs.push_back(btcWalletInt_->setUnconfirmedTarget(kIntConfCount));
   }

   return regIDs;
}

void hd::Leaf::postOnline()
{
   if (firstInit_)
      return;

   unconfTgtRegIds_ = setUnconfirmedTarget();

   const auto &cbTrackAddrChain = [this](bs::sync::SyncState st) {
      if (st != bs::sync::SyncState::Success) {
         if (wct_) {
            wct_->walletReady(walletId());
         }
         return;
      }
      synchronize([this] {
         if (wct_) {
            wct_->walletReady(walletId());
         }
      });
   };
   const bool rc = getAddressTxnCounts([this, cbTrackAddrChain] {
      trackChainAddressUse(cbTrackAddrChain);
   });
}

void hd::Leaf::init(bool force)
{
   if (firstInit_ && !force)
      return;

   if (!armory_ || (armory_->state() != ArmoryState::Ready)) {
      return;
   }
   postOnline();

   if (firstInit_ && force) {
      bs::sync::Wallet::init(force);
   }

   if (activateAddressesInvoked_) {
      return;
   }
   activateAddressesInvoked_ = true;
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
   if (wct_) {
      wct_->walletReset(walletId());
   }
   unconfTgtRegIds_.clear();
}

const std::string& hd::Leaf::walletId() const
{
   return walletId_;
}

const std::string& hd::Leaf::walletIdInt() const
{
   if (isExtOnly_)
      throw std::runtime_error("not internal chain");

   if (walletIdInt_.empty()) {
      for (const auto &c : walletId()) {
         if (isupper(c)) {
            walletIdInt_.push_back(tolower(c));
         }
         else if (islower(c)) {
            walletIdInt_.push_back(toupper(c));
         }
         else {
            walletIdInt_.push_back(c);
         }
      }
   }
   return walletIdInt_;
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
void hd::Leaf::getNewExtAddress(const CbAddress &cb, AddressEntryType aet)
{
   createAddress(cb, aet, false);
}

// Return an internal-facing address.
void hd::Leaf::getNewIntAddress(const CbAddress &cb, AddressEntryType aet)
{
   createAddress(cb, aet, true);
}

// Return a change address.
void hd::Leaf::getNewChangeAddress(const CbAddress &cb, AddressEntryType aet)
{
   createAddress(cb, aet, isExtOnly_ ? false : true);
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
      if (addr.first.path.get(-2) == addrTypeExternal) {
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
      if (addr.first.path.get(-2) == addrTypeInternal) {
         result.push_back(addr.second.id());
      }
   }
   return result;
}

std::vector<std::string> hd::Leaf::registerWallet(
   const std::shared_ptr<ArmoryConnection> &armory, bool asNew)
{
   setArmory(armory);

   if (armory_) {
      const auto addrsExt = getAddrHashesExt();
      const auto addrsInt = isExtOnly_ ? std::vector<BinaryData>{} : getAddrHashesInt();
      std::vector<std::string> regIds;
      auto notifCount = std::make_shared<unsigned>(0);
      const auto &cbRegistered = [this, notifCount](const std::string &)
      {
         if (!isExtOnly_) {
            if ((*notifCount)++ == 0)
               return;
         }
         isRegistered_ = true;
      };

      std::unique_lock<std::mutex> lock(regMutex_);
      btcWallet_ = armory_->instantiateWallet(walletId());
      regIdExt_ = armory_->registerWallet(btcWallet_
         , walletId(), walletId(), addrsExt, cbRegistered, asNew);
      regIds.push_back(regIdExt_);

      if (!isExtOnly_) {
         btcWalletInt_ = armory_->instantiateWallet(walletIdInt());
         regIdInt_ = armory_->registerWallet(btcWalletInt_
            , walletIdInt(), walletId(), addrsInt, cbRegistered, asNew);
         regIds.push_back(regIdInt_);
      }
      logger_->debug("[{}] registered {}+{} addresses in {}, {} regIds {} {}"
         , __func__, addrsExt.size(), addrsInt.size(), walletId(), regIds.size()
         , regIdExt_, regIdInt_);
      return regIds;
   }
   return {};
}

void hd::Leaf::createAddress(const CbAddress &cb, AddressEntryType aet, bool isInternal)
{
   bs::hd::Path addrPath;
   if (isInternal && !isExtOnly_) {
      addrPath.append(addrTypeInternal);
      addrPath.append(lastIntIdx_++);
   }
   else {
      addrPath.append(addrTypeExternal);
      addrPath.append(lastExtIdx_++);
   }
   createAddress(cb, { addrPath, aet });
}

void hd::Leaf::createAddress(const CbAddress &cb, const AddrPoolKey &key)
{
   AddrPoolKey keyCopy = key;
   if (key.aet == AddressEntryType_Default)
      keyCopy.aet = defaultAET_;

   const auto &swapKey = [this, keyCopy](void) -> bs::Address
   {
      bs::Address result;
      const auto addrPoolIt = addressPool_.find(keyCopy);
      if (addrPoolIt != addressPool_.end()) {
         result = std::move(addrPoolIt->second);
         addressPool_.erase(addrPoolIt->first);
         poolByAddr_.erase(result);
      }
      return result;
   };

   const auto &cbAddAddr = [this, cb, keyCopy](const bs::Address &addr) {
      addAddress(addr, keyCopy.path.toString(), keyCopy.aet);
      if (cb) {
         cb(addr);
      }
      if (wct_) {
         wct_->addressAdded(walletId());
      }
   };

   const auto result = swapKey();
   if (result.isNull()) {
      auto topUpCb = [this, keyCopy, swapKey, cbAddAddr, cb](void)
      {
         const auto result = swapKey();
         if (result.isNull()) {
            logger_->error("[{}] failed to find {}/{} after topping up the pool"
               , __func__, keyCopy.path.toString(), (int)keyCopy.aet);
            cb(result);
         }
         else {
            cbAddAddr(result);
         }
      };

      bool extInt = true;
      if (key.path.get(-2) == addrTypeInternal)
         extInt = false;

      topUpAddressPool(extInt, topUpCb);
   }
   else {
      cbAddAddr(result);
   }
}

void hd::Leaf::topUpAddressPool(bool extInt, const std::function<void()> &cb)
{
   if (!signContainer_) {
      logger_->error("[{}] uninited signer container", __func__);
      throw std::runtime_error("uninitialized sign container");
   }

   auto fillUpAddressPoolCallback = [this, extInt, cb](
      const std::vector<std::pair<bs::Address, std::string>>& addrVec)
   {
      /***
      This lambda adds the newly generated addresses to the address pool.

      New addresses generated by topUpAddressPool are not instantiated yet,
      they are only of use for registering the underlying script hashes
      with the DB, which is why they are only saved in the pool.
      ***/

      for (const auto &addrPair : addrVec) {
         const auto path = bs::hd::Path::fromString(addrPair.second);
         addressPool_[{ path, addrPair.first.getType() }] = addrPair.first;
         poolByAddr_[addrPair.first] = { path, addrPair.first.getType() };
      }

      //register new addresses with db
      if (armory_) {
         std::vector<BinaryData> addrHashes;
         for (auto& addrPair : addrVec)
            addrHashes.push_back(addrPair.first.prefixed());

         auto promPtr = std::make_shared<std::promise<bool>>();
         auto fut = promPtr->get_future();
         auto cbRegistered = [promPtr](const std::string &regId)
         {
            promPtr->set_value(true);
         };

         if (extInt) {
            armory_->registerWallet(btcWallet_
               , walletId(), walletId(), addrHashes, cbRegistered, true);
         }
         else {
            armory_->registerWallet(btcWalletInt_
               , walletIdInt(), walletId(), addrHashes, cbRegistered, true);
         }
         fut.wait();
      }

      if (cb)
         cb();
   };

   unsigned lookup = extAddressPoolSize_;
   if (!extInt)
      lookup = intAddressPoolSize_;

   signContainer_->extendAddressChain(
      walletId(), lookup, extInt, fillUpAddressPoolCallback);
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

   const auto path = getPathForAddress(addr);
   if (path.get(-2) == addrTypeInternal) {
      return armory_->getLedgerDelegateForAddress(walletIdInt(), addr);
   }
   else {
      return armory_->getLedgerDelegateForAddress(walletId(), addr);
   }
}

bool hd::Leaf::hasId(const std::string &id) const
{
   return ((walletId() == id) || (!isExtOnly_ && (walletIdInt() == id)));
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

bool hd::Leaf::getSpendableTxOutList(const ArmoryConnection::UTXOsCb &cb, uint64_t val)
{  // process the UTXOs for the purposes of handling internal/external addresses
   const ArmoryConnection::UTXOsCb &cbWrap = [this, cb](const std::vector<UTXO> &utxos) {
      std::vector<UTXO> filteredUTXOs;
      for (const auto &utxo : utxos) {
         const auto nbConf = armory_->getConfirmationsNumber(utxo.getHeight());
         const auto addr = bs::Address::fromUTXO(utxo);
         const auto confCutOff = isExternalAddress(addr) ? kExtConfCount : kIntConfCount;
         if (nbConf >= confCutOff) {
            filteredUTXOs.emplace_back(std::move(utxo));
         }
      }
      if (cb) {
         cb(filteredUTXOs);
      }
   };
   return bs::sync::Wallet::getSpendableTxOutList(cbWrap, val);
}

BTCNumericTypes::balance_type hd::Leaf::getSpendableBalance() const
{
   return (Wallet::getSpendableBalance() - spendableBalanceCorrection_);
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
   const auto path = getPathForAddress(addr);
   if (path.length()) {
      return path.toString();
   }
   return {};
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

void hd::Leaf::merge(const std::shared_ptr<Wallet> walletPtr)
{
   //rudimentary implementation, flesh it out on the go
   auto leafPtr = std::dynamic_pointer_cast<hd::Leaf>(walletPtr);
   if (leafPtr == nullptr)
      throw std::runtime_error("sync::Wallet child class mismatch");

   addrComments_.insert(
      leafPtr->addrComments_.begin(), leafPtr->addrComments_.end());
   txComments_.insert(
      leafPtr->txComments_.begin(), leafPtr->txComments_.end());

   addressPool_ = leafPtr->addressPool_;
   poolByAddr_ = leafPtr->poolByAddr_;

   addressMap_ = leafPtr->addressMap_;
   intAddresses_ = leafPtr->intAddresses_;
   extAddresses_ = leafPtr->extAddresses_;

   lastIntIdx_ = leafPtr->lastIntIdx_;
   lastExtIdx_ = leafPtr->lastExtIdx_;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::XBTLeaf::XBTLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , WalletSignerContainer *container,const std::shared_ptr<spdlog::logger> &logger, bool extOnlyAddresses)
   : Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::Bitcoin, extOnlyAddresses)
{
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , WalletSignerContainer *container,const std::shared_ptr<spdlog::logger> &logger)
   : Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::Authentication, true)
{
   intAddressPoolSize_ = 0;
   extAddressPoolSize_ = 5;
   poolAET_ = { AddressEntryType_P2WPKH };
}

void hd::AuthLeaf::createAddress(const CbAddress &cb, const AddrPoolKey &key)
{
   if (userId_.isNull()) {
      tempAddresses_.insert(key);
      if (cb) {
         cb({});
      }
   }
   else {
      hd::Leaf::createAddress(cb, key);
   }
}

void hd::AuthLeaf::topUpAddressPool(bool extInt, const std::function<void()> &cb)
{
   if (userId_.isNull()) {
      return;
   }

   //auth leaf is ext only
   hd::Leaf::topUpAddressPool(true, cb);
}

void hd::AuthLeaf::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   if (userId.isNull()) {
      reset();
      return;
   }

   for (const auto &addr : tempAddresses_) {
      createAddress([](const bs::Address &) {}, addr);
      lastExtIdx_ = std::max(lastExtIdx_, addr.path.get(-1) + 1);
   }
   topUpAddressPool(true);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::CCLeaf::CCLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , WalletSignerContainer *container, const std::shared_ptr<spdlog::logger> &logger)
   : hd::Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::ColorCoin, true)
   , validationStarted_(false)
   , validationEnded_(false)
{}

hd::CCLeaf::~CCLeaf()
{
   validationStarted_ = false;
}

void hd::CCLeaf::setCCDataResolver(const std::shared_ptr<CCDataResolver> &resolver)
{
   ccResolver_ = resolver;
   setPath(path_);
   checker_ = std::make_shared<TxAddressChecker>(ccResolver_->genesisAddrFor(suffix_), armory_);
}

void hd::CCLeaf::setPath(const bs::hd::Path &path)
{
   hd::Leaf::setPath(path);
   if ((path_.length() > 0) && ccResolver_) {
      suffix_ = ccResolver_->nameByWalletIndex(static_cast<bs::hd::Path::Elem>(path_.get(-1)));
      if (!suffix_.empty()) {
         walletName_ = name_ + "/" + suffix_;
      }
   }
}

void hd::CCLeaf::setArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   hd::Leaf::setArmory(armory);
   if (armory_) {
      act_ = make_unique<CCWalletACT>(armory_.get(), this);
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
      std::unique_lock<std::mutex> lock(addrMapsMtx_);
      addressBalanceMap_.clear();
   }

   if (!ZConly) {
      const auto &cbRefresh = [this](std::vector<UTXO> utxos) {
         const auto &cbUpdateSpendableBalance = [this](const std::vector<UTXO> &spendableUTXOs) {
            std::unique_lock<std::mutex> lock(addrMapsMtx_);
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
      hd::Leaf::getSpendableTxOutList(cbRefresh, UINT64_MAX);
   }

   const auto &cbRefreshZC = [this](const std::vector<UTXO> &utxos) {
      const auto &cbUpdateZcBalance = [this](const std::vector<UTXO> &ZcUTXOs) {
         std::unique_lock<std::mutex> lock(addrMapsMtx_);
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
   hd::Leaf::getSpendableZCList(cbRefreshZC);
}

void hd::CCLeaf::validationProc()
{
   validationStarted_ = true;
   if (!armory_ || (armory_->state() != ArmoryState::Ready)) {
      validationStarted_ = false;
      return;
   }
   validationEnded_ = true;
   refreshInvalidUTXOs();
   hd::Leaf::init();

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
         if (!validationStarted_ || !ccResolver_) {
            return;
         }

         if (ledger == nullptr) {
            logger_->error("[CCLeaf::validationProc::cbLedger] failed to get ledger for : {}"
               , addr.display());
            return;
         }

         const auto &cbCheck = [this, addr, addressesToCheck](const Tx &tx) {
            const auto &cbResult = [this, tx](bool contained) {
               if (!contained && tx.isInitialized()) {
                  invalidTxHash_.insert(tx.getThisHash());
               }
            };
            checker_->containsInputAddress(tx, cbResult, ccResolver_->lotSizeFor(suffix_));

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

            if (empty && wct_) {
               wct_->walletReset(walletId());
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
      getLedgerDelegateForAddress(addr, cbLedger);
   }
}

void hd::CCLeaf::findInvalidUTXOs(const std::vector<UTXO> &utxos, const ArmoryConnection::UTXOsCb &cb)
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
         checker_->containsInputAddress(txData.tx, cbResult, ccResolver_->lotSizeFor(suffix_)
            , txData.utxo.getValue());
      }
   };
   if (txHashes.empty()) {
      cb(utxos);
   }
   else {
      armory_->getTXsByHash(txHashes, cbProcess);
   }
}

void hd::CCLeaf::init(bool force)
{
   if (force) {
      validationStarted_ = false;
   }
   if (checker_ && !validationStarted_) {
      validationEnded_ = false;
      validationProc();
   }
}

void hd::CCLeaf::CCWalletACT::onStateChanged(ArmoryState state)
{
   if (state == ArmoryState::Ready) {
      parent_->init(true);
   }
}

void hd::CCLeaf::onZeroConfReceived(const std::vector<bs::TXEntry> &entries)
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

bool hd::CCLeaf::getSpendableZCList(const ArmoryConnection::UTXOsCb &cb) const
{
   if (validationStarted_ && !validationEnded_) {
      return false;
   }
   const auto &cbZCList = [this, cb](std::vector<UTXO> txOutList) {
      cb(filterUTXOs(txOutList));
   };
   return hd::Leaf::getSpendableZCList(cbZCList);
}

bool hd::CCLeaf::isBalanceAvailable() const
{
   return (validationEnded_ || !checker_) ? hd::Leaf::isBalanceAvailable() : false;
}

BTCNumericTypes::balance_type hd::CCLeaf::correctBalance(BTCNumericTypes::balance_type balance, bool apply) const
{
   if (!ccResolver_ || (ccResolver_->lotSizeFor(suffix_) == 0)) {
      return 0;
   }
   const BTCNumericTypes::balance_type correction = apply ? balanceCorrection_ : 0;
   return (balance - correction) * BTCNumericTypes::BalanceDivider / ccResolver_->lotSizeFor(suffix_);
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

std::vector<uint64_t> hd::CCLeaf::getAddrBalance(const bs::Address &addr) const
{
   if (!ccResolver_ || (ccResolver_->lotSizeFor(suffix_) == 0) || !validationEnded_
      || !Wallet::isBalanceAvailable()) {
      return {};
   }

   /*doesnt seem thread safe, yet addressBalanceMap_ can be changed by other threads*/
   auto inner = [addr, lotSizeInSatoshis= ccResolver_->lotSizeFor(suffix_)]
      (std::vector<uint64_t>& xbtBalances)->void {
      for (auto &balance : xbtBalances) {
         balance /= lotSizeInSatoshis;
      }
   };

   auto balVec = hd::Leaf::getAddrBalance(addr);
   inner(balVec);

   return balVec;
}

bool hd::CCLeaf::isTxValid(const BinaryData &txHash) const
{
   return (invalidTxHash_.find(txHash) == invalidTxHash_.end());
}

BTCNumericTypes::balance_type hd::CCLeaf::getTxBalance(int64_t val) const
{
   if (!ccResolver_) {
      return 0;
   }
   const auto lotSize = ccResolver_->lotSizeFor(suffix_);
   if (lotSize == 0) {
      return 0;
   }
   return (double)val / lotSize;
}

QString hd::CCLeaf::displayTxValue(int64_t val) const
{
   return QLocale().toString(getTxBalance(val), 'f', 0);
}

QString hd::CCLeaf::displaySymbol() const
{
   return suffix_.empty() ? hd::Leaf::displaySymbol() : QString::fromStdString(suffix_);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::SettlementLeaf::SettlementLeaf(const std::string &walletId, const std::string &name, const std::string &desc
   , WalletSignerContainer *container, const std::shared_ptr<spdlog::logger> &logger)
   : Leaf(walletId, name, desc, container, logger, bs::core::wallet::Type::Settlement, true)
{
   intAddressPoolSize_ = 0;
   extAddressPoolSize_ = 0;
   poolAET_ = { AddressEntryType_P2WPKH };
}

void hd::SettlementLeaf::createAddress(const CbAddress &cb, const AddrPoolKey &key)
{
   throw std::runtime_error("Settlement leaves do not yield addresses");
}

void hd::SettlementLeaf::topUpAddressPool(bool extInt, const std::function<void()> &cb)
{
   throw std::runtime_error("Settlement leaves do not yield addresses");
}

void hd::SettlementLeaf::setSettlementID(const SecureBinaryData& id
   , const std::function<void(bool)> &cb)
{
   if (signContainer_ == nullptr) {
      if (cb)
         cb(false);
   }

   signContainer_->setSettlementID(walletId(), id, cb);
}

void hd::SettlementLeaf::getRootPubkey(const std::function<void(const SecureBinaryData &)> &cb) const
{
   if (signContainer_ == nullptr) {
      if (cb)
         cb({});
      return;
   }

   const auto &cbWrap = [cb](bool result, const SecureBinaryData &pubKey) {
      if (cb) {
         cb(result ? pubKey : SecureBinaryData{});
      }
   };
   return signContainer_->getRootPubkey(walletId(), cbWrap);
}
