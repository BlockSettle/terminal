#include "HDLeaf.h"
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrentRun>
#include "CheckRecipSigner.h"
#include "HDNode.h"
#include "SafeLedgerDelegate.h"
#include "Wallets.h"


#define ADDR_KEY     0x00002002

using namespace bs;


hd::BlockchainScanner::BlockchainScanner(const cb_save_to_wallet &cbSave, const cb_completed &cbComplete)
   : cbSaveToWallet_(cbSave), cbCompleted_(cbComplete), processing_(-1), stopped_(false)
{
   threadPool_.setMaxThreadCount(2);
}

hd::BlockchainScanner::~BlockchainScanner()
{
   stop();
}

void hd::BlockchainScanner::init(const std::shared_ptr<Node> &node, const std::string &walletId)
{
   node_ = node;
   walletId_ = walletId;
   rescanWalletId_ = "rescan_" + walletId;
}

void hd::BlockchainScanner::stop()
{
   stopped_ = true;
   threadPool_.clear();
   threadPool_.waitForDone();
}

bs::Address hd::BlockchainScanner::newAddress(const Path &path, AddressEntryType aet)
{
   if (node_ == nullptr) {
      return {};
   }
   const auto addrNode = node_->derive(path, true);
   if (addrNode == nullptr) {
      return {};
   }
   return Address::fromPubKey(addrNode->pubChainedKey(), aet);
}

std::vector<hd::BlockchainScanner::PooledAddress> hd::BlockchainScanner::generateAddresses(
   hd::Path::Elem prefix, hd::Path::Elem start, size_t nb, AddressEntryType aet)
{
   std::vector<PooledAddress> result;
   result.reserve(nb);
   for (Path::Elem i = start; i < start + nb; i++) {
      hd::Path addrPath({ prefix, i });
      const auto &addr = newAddress(addrPath, aet);
      if (!addr.isNull()) {
         result.emplace_back(PooledAddress({ addrPath, aet }, addr));
      }
   }
   return result;
}

std::vector<BinaryData> hd::BlockchainScanner::getRegAddresses(const std::vector<hd::BlockchainScanner::PooledAddress> &src)
{
   std::vector<BinaryData> result;
   for (const auto &addr : src) {
      result.push_back(addr.second.prefixed());
   }
   return result;
}

void hd::BlockchainScanner::fillPortion(Path::Elem start, unsigned int size)
{
   currentPortion_.addresses.clear();
   currentPortion_.addresses.reserve(size * 4);
   currentPortion_.start = start;
   currentPortion_.end = start + size - 1;

   for (hd::Path::Elem addrType : {0, 1}) {
      for (const auto aet : { AddressEntryType_P2SH, AddressEntryType_P2WPKH }) {
         const auto addrs = generateAddresses(addrType, start, size, aet);
         currentPortion_.addresses.insert(currentPortion_.addresses.end(), addrs.begin(), addrs.end());
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
   QtConcurrent::run(&threadPool_, [this] {
      std::shared_ptr<SafeBtcWallet> wlt;
      PyBlockDataManager::instance()->registerWallet(wlt, getRegAddresses(currentPortion_.addresses), rescanWalletId_, true);
   });
}

void hd::BlockchainScanner::onRefresh(const BinaryDataVector &ids)
{
   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }
   const auto it = std::find(ids.get().begin(), ids.get().end(), rescanWalletId_);
   if (it == ids.get().end()) {
      return;
   }
   QtConcurrent::run(&threadPool_, this, &BlockchainScanner::processPortion);
}

void hd::BlockchainScanner::processPortion()
{
   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }

   processing_ = (int)currentPortion_.start;
   currentPortion_.registered = false;
   std::vector<PooledAddress> activeAddresses;

   for (const auto &addr : currentPortion_.addresses) {
      if (stopped_) {
         break;
      }
      const auto ledgerDelegate = PyBlockDataManager::instance()->getLedgerDelegateForScrAddr(rescanWalletId_, addr.second.prefixed());
      if (!ledgerDelegate) {
         continue;
      }
      const auto page0 = ledgerDelegate->getHistoryPage(0);
      if (!page0.empty()) {
         activeAddresses.push_back(addr);
      }
   }
   if (stopped_) {
      return;
   }

   if (!activeAddresses.empty()) {
      if (cbSaveToWallet_) {
         std::sort(activeAddresses.begin(), activeAddresses.end()
            , [](const PooledAddress &a, const PooledAddress &b) { return (a.first.path < b.first.path); });
         cbSaveToWallet_(activeAddresses);
      }
      if (cbWriteLast_) {
         cbWriteLast_(walletId_, currentPortion_.end + 1);
      }
      fillPortion(currentPortion_.end + 1, portionSize_);
      currentPortion_.registered = true;
      std::shared_ptr<SafeBtcWallet> wlt;
      PyBlockDataManager::instance()->registerWallet(wlt, getRegAddresses(currentPortion_.addresses), rescanWalletId_, false);
   }
   else {
      currentPortion_.start = currentPortion_.end = 0;
      currentPortion_.addresses.clear();
      processing_ = -1;

      if (cbWriteLast_) {
         cbWriteLast_(walletId_, UINT32_MAX);
      }
      if (cbCompleted_) {
         cbCompleted_();
      }
   }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::Leaf::Leaf(const std::string &name, const std::string &desc, bs::wallet::Type type
   , bool extOnlyAddresses)
   : bs::Wallet()
   , hd::BlockchainScanner([this](const std::vector<PooledAddress> &addrs) { onSaveToWallet(addrs); }
      , [this] { onScanComplete(); })
   , type_(type), name_(name), desc_(desc), isExtOnly_(extOnlyAddresses)
{ }

hd::Leaf::~Leaf()
{
   stop();
   inited_ = false;
}

void hd::Leaf::stop()
{
   hd::BlockchainScanner::stop();
   bs::Wallet::stop();
}

void hd::Leaf::SetBDM(const std::shared_ptr<PyBlockDataManager> &bdm)
{
   bs::Wallet::SetBDM(bdm);
   if (bdm_) {
      connect(bdm_.get(), &PyBlockDataManager::zeroConfReceived, this, &hd::Leaf::onZeroConfReceived, Qt::QueuedConnection);
      connect(bdm.get(), &PyBlockDataManager::refreshed, this, &hd::Leaf::onRefresh, Qt::QueuedConnection);
   }
}

void hd::Leaf::init(const std::shared_ptr<Node> &node, const hd::Path &path, const std::shared_ptr<Node> &rootNode)
{
   setRootNode(rootNode);

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

   if (node) {
      if (node != node_) {
         node_ = node;
         inited_ = true;
         hd::BlockchainScanner::init(node_, GetWalletId());
      }
   }
   else {
      reset();
      inited_ = false;
   }
}

void hd::Leaf::onZeroConfReceived(const std::vector<LedgerEntryData> &led)
{
   activateAddressesFromLedger(led);
}

void hd::Leaf::onRefresh(const BinaryDataVector &ids)
{
   hd::BlockchainScanner::onRefresh(ids);
}

void hd::Leaf::firstInit()
{
   bs::Wallet::firstInit();

   if (activateAddressesInvoked_ || !bdm_) {
      return;
   }
   activateAddressesInvoked_ = true;
   activateAddressesFromLedger(bdm_->getWalletsHistory({ GetWalletId() }));
}

void hd::Leaf::activateAddressesFromLedger(const std::vector<LedgerEntryData> &led)
{
   bool activated = false;
   for (const auto &entry : led) {
      const auto tx = bdm_->getTxByHash(entry.getTxHash());
      if (!tx.isInitialized()) {
         continue;
      }
      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy((int)i);
         const auto addr = bs::Address::fromTxOut(out);
         if (containsHiddenAddress(addr)) {
            activated = true;
            activateHiddenAddress(addr);
         }
      }
      for (size_t i = 0; i < tx.getNumTxIn(); i++) {
         auto in = tx.getTxInCopy((int)i);
         OutPoint op = in.getOutPoint();
         Tx prevTx = bdm_->getTxByHash(op.getTxHash());
         if (prevTx.isInitialized()) {
            const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy(op.getTxOutIndex()));
            if (containsHiddenAddress(addr)) {
               activated = true;
               activateHiddenAddress(addr);
            }
         }
      }
   }
   if (activated) {
      emit addressAdded();
   }
}

void hd::Leaf::activateHiddenAddress(const bs::Address &addr)
{
   const auto itAddr = poolByAddr_.find(addr);
   if (itAddr == poolByAddr_.end()) {
      return;
   }
   createAddressWithPath(itAddr->second.path, itAddr->second.aet, false);
}

bool hd::Leaf::copyTo(std::shared_ptr<hd::Leaf> &leaf) const
{
   for (const auto &addr : addressMap_) {
      const auto &address = std::get<0>(addr.second);
      const auto newAddr = leaf->createAddress(std::get<2>(addr.second), addr.first, address.getType(), false);
      const auto comment = GetAddressComment(address);
      if (!comment.empty()) {
         if (!leaf->SetAddressComment(newAddr, comment)) {
         }
      }
   }
   for (const auto &addr : tempAddresses_) {
      leaf->createAddress(std::get<0>(addr.second), addr.first, std::get<1>(addr.second), false);
   }
   leaf->path_ = path_;
   leaf->lastExtIdx_ = lastExtIdx_;
   leaf->lastIntIdx_ = lastIntIdx_;
   return true;
}

void hd::Leaf::reset()
{
   node_ = nullptr;
   lastIntIdx_ = lastExtIdx_ = 0;
   addressMap_.clear();
   usedAddresses_.clear();
   intAddresses_.clear();
   extAddresses_.clear();
   addrToIndex_.clear();
   addrPrefixedHashes_.clear();
   addressHashes_.clear();
   hashToPubKey_.clear();
   pubKeyToPath_.clear();
   addressPool_.clear();
   poolByAddr_.clear();
   emit walletReset();
}

std::string hd::Leaf::GetWalletId() const
{
   return (inited_ && node_) ? node_->getId() : "";
}

std::string hd::Leaf::GetWalletDescription() const
{
   return desc_;
}

wallet::EncryptionType hd::Leaf::encryptionType() const
{
   if (!rootNode_) {
      return wallet::EncryptionType::Unencrypted;
   }
   return rootNode_->encType();
}

bool hd::Leaf::containsAddress(const bs::Address &addr)
{
   return (getAddressIndexForAddr(addr) != UINT32_MAX);
}

bool hd::Leaf::containsHiddenAddress(const bs::Address &addr) const
{
   return (poolByAddr_.find(addr) != poolByAddr_.end());
}

BinaryData hd::Leaf::getRootId() const
{
   if (!node_) {
      return {};
   }
   return node_->pubCompressedKey();
}

bs::Address hd::Leaf::GetNewExtAddress(AddressEntryType aet)
{
   return createAddress(aet, false);
}

bs::Address hd::Leaf::GetNewChangeAddress(AddressEntryType aet)
{
   return createAddress(aet, isExtOnly_ ? false : true);
}

bs::Address hd::Leaf::GetRandomChangeAddress(AddressEntryType aet)
{
   if (isExtOnly_) {
      if (extAddresses_.empty()) {
         return GetNewExtAddress(aet);
      } else if (extAddresses_.size() == 1) {
         return extAddresses_[0];
      }
      return extAddresses_[qrand() % extAddresses_.size()];
   }
   else {
      if (!lastIntIdx_) {
         return GetNewChangeAddress(aet);
      }
      else {
         return intAddresses_[qrand() % intAddresses_.size()];
      }
   }
}

std::shared_ptr<AddressEntry> hd::Leaf::getAddressEntryForAddr(const BinaryData &addr)
{
   const auto index = getAddressIndexForAddr(addr);
   if (index == UINT32_MAX) {
      return nullptr;
   }

   const auto &itAddr = addressMap_.find(index);
   assert(itAddr != addressMap_.end());

   const auto addrPair = itAddr->second;
   const auto asset = std::get<1>(addrPair)->getAsset(-1);
   const auto addrType = std::get<0>(addrPair).getType();
   return getAddressEntryForAsset(asset, addrType);
}

void hd::Leaf::addAddress(const bs::Address &addr, const BinaryData &pubChainedKey, const hd::Path &path)
{
   hashToPubKey_[BtcUtils::getHash160(pubChainedKey)] = pubChainedKey;
   if (addr.getType() == AddressEntryType_P2SH) {
      hashToPubKey_[addr.unprefixed()] = addr.getWitnessScript();
   }
   pubKeyToPath_[pubChainedKey] = path;
}

std::shared_ptr<hd::Node> hd::Leaf::getNodeForAddr(const bs::Address &addr) const
{
   if (addr.isNull()) {
      return nullptr;
   }
   const auto index = getAddressIndex(addr);
   if (index == UINT32_MAX) {
      return nullptr;
   }
   const auto itTuple = addressMap_.find(index);
   if (itTuple == addressMap_.end()) {
      return nullptr;
   }
   return std::get<1>(itTuple->second);
}

SecureBinaryData hd::Leaf::GetPublicKeyFor(const bs::Address &addr)
{
   const auto node = getNodeForAddr(addr);
   if (node == nullptr) {
      return BinaryData();
   }
   return node->pubCompressedKey();
}

SecureBinaryData hd::Leaf::GetPubChainedKeyFor(const bs::Address &addr)
{
   const auto node = getNodeForAddr(addr);
   if (node == nullptr) {
      return BinaryData();
   }
   return node->pubChainedKey();
}

std::shared_ptr<hd::Node> hd::Leaf::GetPrivNodeFor(const bs::Address &addr, const SecureBinaryData &password)
{
   if (isWatchingOnly()) {
      return nullptr;
   }
   const auto addrPath = getPathForAddress(addr);
   if (!addrPath.length()) {
      return nullptr;
   }
   std::shared_ptr<hd::Node> leafNode;
   if (rootNode_->encType() != wallet::EncryptionType::Unencrypted) {
      if (password.isNull()) {
         return nullptr;
      }
      const auto decrypted = rootNode_->decrypt(password);
      leafNode = decrypted->derive(path_);
   }
   else {
      leafNode = rootNode_->derive(path_);
   }
   return leafNode->derive(addrPath);
}

KeyPair hd::Leaf::GetKeyPairFor(const bs::Address &addr, const SecureBinaryData &password)
{
   const auto &node = GetPrivNodeFor(addr, password);
   if (!node) {
      return {};
   }
   return { node->privChainedKey(), node->pubChainedKey() };
}

void hd::Leaf::setDB(const std::shared_ptr<LMDBEnv> &dbEnv, LMDB *db)
{
   if (dbEnv && db && (!db_ || !dbEnv_)) {
      MetaData::readFromDB(dbEnv, db);
      MetaData::write(dbEnv, db);
   }
   dbEnv_ = dbEnv;
   db_ = db;
}

AddressEntryType hd::Leaf::getAddrTypeForAddr(const BinaryData &addr)
{
   const auto addrEntry = getAddressEntryForAddr(addr);
   if (addrEntry == nullptr) {
      return AddressEntryType_Default;
   }
   return addrEntry->getType();
}

std::set<BinaryData> hd::Leaf::getAddrHashSet()
{
   std::set<BinaryData> result = addrPrefixedHashes_;
   for (const auto &addr : addressPool_) {
      result.insert(addr.second.id());
   }
   return result;
}

bs::Address hd::Leaf::createAddress(AddressEntryType aet, bool isInternal)
{
   topUpAddressPool();
   hd::Path addrPath;
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
   return createAddress(addrPath, lastIntIdx_ + lastExtIdx_, aet);
}

bs::Address hd::Leaf::createAddress(const Path &path, Path::Elem index, AddressEntryType aet, bool signal)
{
   const auto addrNode = node_ ? node_->derive(path, true) : nullptr;
   if (addrNode == nullptr) {
      return {};
   }
   const bool isInternal = (path.get(-2) == addrTypeInternal);
   if (isInternal && isExtOnly_) {
      return {};
   }
   bs::Address result;

   AddressEntryType addrType = aet;
   if (aet == AddressEntryType_Default) {
      addrType = defaultAET_;
   }
   const auto addrPoolIt = addressPool_.find({ path, addrType });
   if (addrPoolIt != addressPool_.end()) {
      result = std::move(addrPoolIt->second);
      addressPool_.erase(addrPoolIt->first);
      poolByAddr_.erase(result);
   }
   else {
      result = Address::fromPubKey(addrNode->pubChainedKey(), addrType);
   }

   if (addrToIndex_.find(result.unprefixed()) != addrToIndex_.end()) {
      return result;
   }

   const auto complementaryAddrType = (aet == AddressEntryType_P2SH) ? AddressEntryType_P2WPKH : AddressEntryType_P2SH;
   const auto &complementaryAddr = Address::fromPubKey(addrNode->pubChainedKey(), complementaryAddrType);

   if (isInternal) {
      intAddresses_.push_back(result);
   }
   else {
      extAddresses_.push_back(result);
   }
   usedAddresses_.push_back(result);
   addressMap_[index] = AddressTuple(result, addrNode, path);
   addrToIndex_[result.unprefixed()] = index;
   addrToIndex_[complementaryAddr.unprefixed()] = index;
   addrPrefixedHashes_.insert(result.id());
   addrPrefixedHashes_.insert(complementaryAddr.id());
   addressHashes_.insert(result.unprefixed());
   addressHashes_.insert(complementaryAddr.unprefixed());
   addAddress(result, addrNode->pubChainedKey(), path);
   addAddress(complementaryAddr, addrNode->pubChainedKey(), path);
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

std::shared_ptr<AddressEntry> hd::Leaf::getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
   , AddressEntryType ae_type)
{
   if (ae_type == AddressEntryType_Default) {
      ae_type = defaultAET_;
   }

   shared_ptr<AddressEntry> aePtr = nullptr;
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
}

hd::Path::Elem hd::Leaf::getAddressIndexForAddr(const BinaryData &addr) const
{
   bs::Address p2pk(addr, AddressEntryType_P2PKH);
   bs::Address p2sh(addr, AddressEntryType_P2SH);
   Path::Elem index = UINT32_MAX;
   for (const auto &bd : { p2pk.unprefixed(), p2sh.unprefixed() }) {
      const auto itIndex = addrToIndex_.find(bd);
      if (itIndex != addrToIndex_.end()) {
         index = itIndex->second;
         break;
      }
   }
   return index;
}

hd::Path::Elem hd::Leaf::getAddressIndex(const bs::Address &addr) const
{
   auto itIndex = addrToIndex_.find(addr.prefixed());
   if (itIndex == addrToIndex_.end()) {
      itIndex = addrToIndex_.find(addr.unprefixed());
      if (itIndex == addrToIndex_.end()) {
         return UINT32_MAX;
      }
   }
   return itIndex->second;
}

hd::Path hd::Leaf::getPathForAddress(const bs::Address &addr) const
{
   const auto index = getAddressIndex(addr);
   if (index == UINT32_MAX) {
      return {};
   }
   const auto addrIt = addressMap_.find(index);
   if (addrIt == addressMap_.end()) {
      return {};
   }
   const auto path = std::get<2>(addrIt->second);
   if (path.length() < 2) {
      return {};
   }
   return path;
}

std::vector<UTXO> hd::Leaf::getSpendableTxOutList(uint64_t val) const
{
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return {};
   }
   std::vector<UTXO> result;
   for (const auto &utxo : bs::Wallet::getSpendableTxOutList(val)) {
      const auto &addr = bs::Address::fromUTXO(utxo);
      const auto &path = getPathForAddress(addr);
      if (path.length() < 2) {
         continue;
      }
      const uint32_t nbConf = (path.get(-2) == addrTypeExternal) ? 6 : 1;
      if (utxo.getNumConfirm(bdm->GetTopBlockHeight()) >= nbConf) {
         result.emplace_back(utxo);
      }
   }
   return result;
}

std::string hd::Leaf::GetAddressIndex(const bs::Address &addr)
{
   return getPathForAddress(addr).toString(false);
}

bool hd::Leaf::IsExternalAddress(const Address &addr) const
{
   const auto &path = getPathForAddress(addr);
   if (path.length() < 2) {
      return false;
   }
   return (path.get(-2) == addrTypeExternal);
}

bool hd::Leaf::AddressIndexExists(const std::string &index) const
{
   const auto path = hd::Path::fromString(index);
   if (path.length() < 2) {
      return false;
   }
   for (const auto &addr : addressMap_) {
      if (std::get<2>(addr.second) == path) {
         return true;
      }
   }
   return false;
}

bs::Address hd::Leaf::CreateAddressWithIndex(const std::string &index, AddressEntryType aet, bool signal)
{
   return createAddressWithPath(hd::Path::fromString(index), aet, signal);
}

bs::Address hd::Leaf::createAddressWithPath(const hd::Path &path, AddressEntryType aet, bool signal)
{
   if (path.length() < 2) {
      return {};
   }
   auto addrPath = path;
   if (path.length() > 2) {
      addrPath.clear();
      addrPath.append(path.get(-2));
      addrPath.append(path.get(-1));
   }
   for (const auto &addr : addressMap_) {
      if (std::get<2>(addr.second) == addrPath) {
         return std::get<0>(addr.second);
      }
   }
   auto &lastIndex = (path.get(-2) == addrTypeInternal) ? lastIntIdx_ : lastExtIdx_;
   const auto addrIndex = path.get(-1);
   const int nbAddresses = addrIndex - lastIndex;
   if (nbAddresses > 0) {
      for (const auto &addr : generateAddresses(path.get(-2), lastIndex, nbAddresses, aet)) {
         lastIndex++;
         createAddress(addr.first.path, lastIntIdx_ + lastExtIdx_, aet, signal);
      }
   }
   lastIndex++;
   return createAddress(addrPath, lastIntIdx_ + lastExtIdx_, aet, signal);
}

void hd::Leaf::onSaveToWallet(const std::vector<PooledAddress> &addresses)
{
   for (const auto &addr : addresses) {
      createAddressWithPath(addr.first.path, addr.first.aet, false);
   }
   if (!addresses.empty()) {
      emit addressAdded();
   }
}

void hd::Leaf::onScanComplete()
{
   if (cbScanNotify_) {
      cbScanNotify_(index(), (GetUsedAddressCount() > 0));
   }
   emit scanComplete(GetWalletId());
   topUpAddressPool();
   RegisterWallet(bdm_);
}

hd::Path::Elem hd::Leaf::getLastAddrPoolIndex(Path::Elem addrType) const
{
   Path::Elem result = 0;
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

void hd::Leaf::serializeAddr(BinaryWriter &bw, Path::Elem index, AddressEntryType aet, const Path &path)
{
   bw.put_uint32_t(ADDR_KEY);
   bw.put_uint32_t(index);
   bw.put_uint32_t(aet);

   BinaryData addrPath(path.toString(false));
   bw.put_var_int(addrPath.getSize());
   bw.put_BinaryData(addrPath);
}

BinaryData hd::Leaf::serialize() const
{
   BinaryWriter bw;
   bw.put_var_int(1);   // format revision - should always be <= 10

   BinaryData index(path_.toString(false));
   bw.put_var_int(index.getSize());
   bw.put_BinaryData(index);

   const auto node = serializeNode();
   bw.put_var_int(node.getSize());
   bw.put_BinaryData(node);

   bw.put_uint32_t(lastExtIdx_);
   bw.put_uint32_t(lastIntIdx_);

   for (const auto &addr : addressMap_) {
      serializeAddr(bw, addr.first, std::get<0>(addr.second).getType(), std::get<2>(addr.second));
   }
   for (const auto &addr : tempAddresses_) {
      serializeAddr(bw, addr.first, addr.second.second, addr.second.first);
   }

   if (!addressPool_.empty()) {
      bw.put_uint8_t(getLastAddrPoolIndex(addrTypeInternal) - lastIntIdx_ + 1);
      bw.put_uint8_t(getLastAddrPoolIndex(addrTypeExternal) - lastExtIdx_ + 1);
   }

   BinaryWriter finalBW;
   finalBW.put_var_int(bw.getSize());
   finalBW.put_BinaryData(bw.getData());
   return finalBW.getData();
}

bool hd::Leaf::deserialize(const BinaryData &ser, const std::shared_ptr<hd::Node> &rootNode)
{
   BinaryRefReader brr(ser);
   bool oldFormat = true;
   auto len = brr.get_var_int();
   if (len <= 10) {
      len = brr.get_var_int();
      oldFormat = false;
   }
   auto strPath = brr.get_BinaryData(len).toBinStr();
   auto path = Path::fromString(strPath);
   std::shared_ptr<hd::Node> node;
   if (oldFormat) {
      if (rootNode) {
         node = rootNode->derive(path);
      }
   }
   else {
      len = brr.get_var_int();
      BinaryData serNode = brr.get_BinaryData(len);
      node = hd::Node::deserialize(serNode);
   }
   init(node, path, rootNode);
   lastExtIdx_ = brr.get_uint32_t();
   lastIntIdx_ = brr.get_uint32_t();

   while (brr.getSizeRemaining() >= 10) {
      const auto keyAddr = brr.get_uint32_t();
      if (keyAddr != ADDR_KEY) {
         return false;
      }
      const auto index = brr.get_uint32_t();
      const auto addrType = static_cast<AddressEntryType>(brr.get_uint32_t());

      len = brr.get_var_int();
      strPath = brr.get_BinaryData(len).toBinStr();
      path = Path::fromString(strPath);
      hd::Path addrPath;
      if (path.length() <= 2) {
         addrPath = path;
      }
      else {
         addrPath.append(path.get(-2));
         addrPath.append(path.get(-1));
      }
      const auto &addr = createAddress(addrPath, index, addrType, false);
      if (!addr.isNull()) {
         const auto actualIdx = addrPath.get(-1);
         if (addrPath.get(0) == addrTypeExternal) {
            lastExtIdx_ = qMax(lastExtIdx_, actualIdx + 1);
         }
         else {
            lastIntIdx_ = qMax(lastIntIdx_, actualIdx + 1);
         }
      }
   }
   if (node_) {
      if (brr.getSizeRemaining() >= 2) {
         const auto nbIntAddresses = brr.get_uint8_t();
         const auto nbExtAddresses = brr.get_uint8_t();
         topUpAddressPool(nbIntAddresses, nbExtAddresses);
      }
      else {
         topUpAddressPool();
      }
   }
   return true;
}


class LeafResolver : public ResolverFeed
{
public:
   using BinaryDataMap = std::unordered_map<BinaryData, BinaryData>;

   LeafResolver(const BinaryDataMap &map) : hashToPubKey_(map) {}

   BinaryData getByVal(const BinaryData& key) override {
      const auto itKey = hashToPubKey_.find(key);
      if (itKey == hashToPubKey_.end()) {
         throw std::runtime_error("hash not found");
      }
      return itKey->second;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey) override {
      throw std::runtime_error("no privkey");
      return {};
   }

private:
   const BinaryDataMap hashToPubKey_;
};

class LeafSigningResolver : public LeafResolver
{
public:
   LeafSigningResolver(const BinaryDataMap &map, const SecureBinaryData &password
      , const hd::Path &rootPath, const std::shared_ptr<hd::Node> &rootNode
      , const std::unordered_map<BinaryData, hd::Path> &pathMap)
      : LeafResolver(map), password_(password), rootPath_(rootPath), rootNode_(rootNode), pathMap_(pathMap) {}

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey) override {
      privKey_.clear();
      const auto pathIt = pathMap_.find(pubkey);
      if (pathIt == pathMap_.end()) {
         throw std::runtime_error("no pubkey found");
      }
      std::shared_ptr<hd::Node> leafNode;
      if (rootNode_->encType() != wallet::EncryptionType::Unencrypted) {
         if (password_.isNull()) {
            throw std::runtime_error("no password for encrypted key");
         }
         const auto decrypted = rootNode_->decrypt(password_);
         leafNode = decrypted->derive(rootPath_);
      }
      else {
         leafNode = rootNode_->derive(rootPath_);
      }
      const auto addrNode = leafNode->derive(pathIt->second);
      privKey_ = addrNode->privChainedKey();
      return privKey_;
   }

private:
   const SecureBinaryData  password_;
   const hd::Path          rootPath_;
   SecureBinaryData        privKey_;
   const std::shared_ptr<hd::Node>                 rootNode_;
   const std::unordered_map<BinaryData, hd::Path>  pathMap_;
};


std::shared_ptr<ResolverFeed> hd::Leaf::GetResolver(const SecureBinaryData &password)
{
   if (isWatchingOnly()) {
      return nullptr;
   }
   return std::make_shared<LeafSigningResolver>(hashToPubKey_, password, path_, rootNode_, pubKeyToPath_);
}

std::shared_ptr<ResolverFeed> hd::Leaf::GetPublicKeyResolver()
{
   return std::make_shared<LeafResolver>(hashToPubKey_);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(const std::string &name, const std::string &desc)
   : Leaf(name, desc, bs::wallet::Type::Authentication)
{
   intAddressPoolSize_ = 0;
   extAddressPoolSize_ = 0;
}

void hd::AuthLeaf::init(const std::shared_ptr<Node> &node, const hd::Path &path, const std::shared_ptr<Node> &rootNode)
{
   const auto prevNode = node_;
   hd::Leaf::init(node, path, rootNode);
   if (unchainedNode_ != node) {
      if (node_ && !unchainedNode_) {
         unchainedNode_ = node_;
      }
      node_ = nullptr;
   }
   else {
      node_ = prevNode;
   }
   inited_ = (node_ != nullptr);
}

void hd::AuthLeaf::setRootNode(const std::shared_ptr<hd::Node> &rootNode)
{
   unchainedRootNode_ = rootNode;
}

bs::Address hd::AuthLeaf::createAddress(const Path &path, Path::Elem index, AddressEntryType aet
   , bool signal)
{
   if (userId_.isNull()) {
      tempAddresses_[index] = { path, aet };
      return {};
   }
   return hd::Leaf::createAddress(path, index, aet, signal);
}

void hd::AuthLeaf::SetUserID(const BinaryData &userId)
{
   userId_ = userId;
   if (userId.isNull()) {
      reset();
      inited_ = false;
      return;
   }

   if (unchainedRootNode_) {
      rootNode_ = std::make_shared<bs::hd::ChainedNode>(*unchainedRootNode_, userId);
   }
   if (unchainedNode_) {
      node_ = std::make_shared<hd::ChainedNode>(*unchainedNode_, userId);

      for (const auto &addr : tempAddresses_) {
         const auto &path = addr.second.first;
         createAddress(path, addr.first, addr.second.second, false);
         lastExtIdx_ = qMax(lastExtIdx_, path.get(-1) + 1);
      }
      hd::BlockchainScanner::init(node_, GetWalletId());
      const auto poolAddresses = generateAddresses(addrTypeExternal, lastExtIdx_, 5, AddressEntryType_P2WPKH);
      for (const auto &addr : poolAddresses) {
         addressPool_[addr.first] = addr.second;
         poolByAddr_[addr.second] = addr.first;
      }
   }
   inited_ = (node_ != nullptr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::CCLeaf::CCLeaf(const std::string &name, const std::string &desc, bool extOnlyAddresses)
   : hd::Leaf(name, desc, bs::wallet::Type::ColorCoin, extOnlyAddresses)
   , validationStarted_(false), validationEnded_(false)
{
   threadPool_.setMaxThreadCount(1);
   const auto &bdm = PyBlockDataManager::instance();
   if (bdm) {
      connect(bdm.get(), &PyBlockDataManager::zeroConfReceived, this, &hd::CCLeaf::onZeroConfReceived);
      connect(bdm.get(), &PyBlockDataManager::refreshed, this, &hd::CCLeaf::onRefresh, Qt::QueuedConnection);
   }
}

hd::CCLeaf::~CCLeaf()
{
   validationStarted_ = false;
   threadPool_.clear();
   threadPool_.waitForDone();
}

void hd::CCLeaf::setData(const std::string &data)
{
   checker_ = std::make_shared<TxAddressChecker>(bs::Address(data));
}

void hd::CCLeaf::UpdateBalanceFromDB()
{
   if (!validationEnded_) {
      return;
   }
   hd::Leaf::UpdateBalanceFromDB();

   QMutexLocker lock(&addrMapsMtx_);
   if (updateAddrBalance_ && isBalanceAvailable()) {
      updateAddrBalance_ = false;
      addressBalanceMap_.clear();
      for (const auto &utxo : getSpendableTxOutList()) {
         auto &balanceVec = addressBalanceMap_[utxo.getRecipientScrAddr()];
         if (balanceVec.empty()) {
            balanceVec = { 0, 0, 0 };
         }
         balanceVec[0] += utxo.getValue();
         balanceVec[1] += utxo.getValue();
      }

      for (const auto &utxo : getSpendableZCList()) {
         auto &balanceVec = addressBalanceMap_[utxo.getRecipientScrAddr()];
         if (balanceVec.empty()) {
            balanceVec = { 0, 0, 0 };
         }
         balanceVec[2] += utxo.getValue();
         balanceVec[0] = balanceVec[1] + balanceVec[2];
      }
   }
}

void hd::CCLeaf::validationProc()
{
   validationStarted_ = true;
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm || (bdm->GetState() != PyBlockDataManagerState::Ready)) {
      validationStarted_ = false;
      return;
   }
   findInvalidUTXOs(hd::Leaf::getSpendableTxOutList());
   findInvalidUTXOs(hd::Leaf::getSpendableZCList());
   validationEnded_ = true;
   hd::Leaf::firstInit();
   emit addressAdded();

   if (!validationStarted_) {
      return;
   }

   for (const auto &address : GetUsedAddressList()) {
      if (!validationStarted_) {
         return;
      }
      const auto ledgerDelegate = bdm->getLedgerDelegateForScrAddr(GetWalletId(), address.id());
      if (!ledgerDelegate) {
         continue;
      }
      const auto &page = ledgerDelegate->getHistoryPage(0);
      if (page.empty()) {
         continue;
      }
      for (const auto &led : page) {
         if (!isTxValid(led.getTxHash())) {
            continue;
         }
         const auto &tx = bdm->getTxByHash(led.getTxHash());
         if (!tx.isInitialized() || !checker_->containsInputAddress(tx, lotSizeInSatoshis_)) {
            invalidTxHash_.insert(led.getTxHash());
         }
      }
   }
   emit walletReset();
}

void hd::CCLeaf::findInvalidUTXOs(const std::vector<UTXO> &utxos)
{
   uint64_t invalidBalance = 0;
   for (const auto &utxo : utxos) {
      const auto bdm = PyBlockDataManager::instance();
      if (!validationStarted_ || !bdm) {
         return;
      }
      const auto &hash = utxo.getTxHash();
      const auto &tx = bdm->getTxByHash(hash);
      if (!tx.isInitialized() || !checker_->containsInputAddress(tx, lotSizeInSatoshis_, utxo.getValue())) {
         invalidTx_.insert(utxo);
         invalidTxHash_.insert(hash);
         invalidBalance += utxo.getValue();
      }
   }
   balanceCorrection_ += invalidBalance / BTCNumericTypes::BalanceDivider;
}

void hd::CCLeaf::firstInit()
{
   if (checker_ && !validationStarted_) {
      validationEnded_ = false;
      QtConcurrent::run(&threadPool_, this, &hd::CCLeaf::validationProc);
   }
}

void hd::CCLeaf::onRefresh(const BinaryDataVector &ids)
{
   hd::Leaf::onRefresh(ids);
   const auto it = std::find(ids.get().begin(), ids.get().end(), GetWalletId());
   if (it == ids.get().end()) {
      return;
   }
   firstInit();
}

void hd::CCLeaf::onZeroConfReceived(const std::vector<LedgerEntryData> &led)
{
   hd::Leaf::onZeroConfReceived(led);
   findInvalidUTXOs(hd::Leaf::getSpendableZCList());
   UpdateBalanceFromDB();
   emit addressAdded();
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

std::vector<UTXO> hd::CCLeaf::getSpendableTxOutList(uint64_t val) const
{
   if (validationStarted_ && !validationEnded_) {
      return {};
   }
   return filterUTXOs(hd::Leaf::getSpendableTxOutList(val));
}

std::vector<UTXO> hd::CCLeaf::getSpendableZCList() const
{
   if (validationStarted_ && !validationEnded_) {
      return {};
   }
   return filterUTXOs(hd::Leaf::getSpendableZCList());
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

BTCNumericTypes::balance_type hd::CCLeaf::GetSpendableBalance() const
{
   return correctBalance(hd::Leaf::GetSpendableBalance());
}

BTCNumericTypes::balance_type hd::CCLeaf::GetUnconfirmedBalance() const
{
   return correctBalance(hd::Leaf::GetUnconfirmedBalance(), false);
}

BTCNumericTypes::balance_type hd::CCLeaf::GetTotalBalance() const
{
   return correctBalance(hd::Leaf::GetTotalBalance());
}

std::vector<uint64_t> hd::CCLeaf::getAddrBalance(const bs::Address &addr) const
{
   if (!lotSizeInSatoshis_ || !validationEnded_) {
      return { 0, 0, 0 };
   }
   const auto it = addressBalanceMap_.find(addr.prefixed());
   if (it == addressBalanceMap_.end()) {
      return { 0, 0, 0 };
   }
   auto xbtBalances = it->second;
   for (auto &balance : xbtBalances) {
      balance /= lotSizeInSatoshis_;
   }
   return xbtBalances;
}

bool hd::CCLeaf::isTxValid(const BinaryData &txHash) const
{
   return (invalidTxHash_.find(txHash) == invalidTxHash_.end());
}

BTCNumericTypes::balance_type hd::CCLeaf::GetTxBalance(int64_t val) const
{
   if (!lotSizeInSatoshis_) {
      return 0;
   }
   return (double)val / lotSizeInSatoshis_;
}

QString hd::CCLeaf::displayTxValue(int64_t val) const
{
   return QLocale().toString(GetTxBalance(val), 'f', 0);
}

QString hd::CCLeaf::displaySymbol() const
{
   return suffix_.empty() ? hd::Leaf::displaySymbol() : QString::fromStdString(suffix_);
}
