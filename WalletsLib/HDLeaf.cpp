#include "HDLeaf.h"
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrentRun>
#include "CheckRecipSigner.h"
#include "HDNode.h"
#include "Wallets.h"


#define ADDR_KEY     0x00002002

using namespace bs;


hd::BlockchainScanner::BlockchainScanner(const cb_save_to_wallet &cbSave, const cb_completed &cbComplete)
   : cbSaveToWallet_(cbSave), cbCompleted_(cbComplete), processing_(-1)
{}

void hd::BlockchainScanner::init(const std::shared_ptr<Node> &node, const std::string &walletId)
{
   node_ = node;
   walletId_ = walletId;
   rescanWalletId_ = "rescan_" + walletId;
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

   std::shared_ptr<AsyncClient::BtcWallet> wlt;
   armoryConn_->registerWallet(wlt, rescanWalletId_, getRegAddresses(currentPortion_.addresses), [] {}, true);
}

void hd::BlockchainScanner::onRefresh(const std::vector<BinaryData> &ids)
{
   if (!currentPortion_.registered || (processing_ == (int)currentPortion_.start)) {
      return;
   }
   const auto it = std::find(ids.begin(), ids.end(), rescanWalletId_);
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
   auto pooledAddresses = new std::map<Address, PooledAddress>;

   const auto &cbProcess = [this] {
      if (!currentPortion_.activeAddresses.empty()) {
         if (cbSaveToWallet_) {
            std::sort(currentPortion_.activeAddresses.begin(), currentPortion_.activeAddresses.end()
               , [](const PooledAddress &a, const PooledAddress &b) { return (a.first.path < b.first.path); });
            cbSaveToWallet_(currentPortion_.activeAddresses);
         }
         if (cbWriteLast_) {
            cbWriteLast_(walletId_, currentPortion_.end + 1);
         }
         fillPortion(currentPortion_.end + 1, portionSize_);
         currentPortion_.registered = true;
         std::shared_ptr<AsyncClient::BtcWallet> wlt;
         armoryConn_->registerWallet(wlt, rescanWalletId_, getRegAddresses(currentPortion_.addresses), false);
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
   };
   const auto &cbDelegates = [this, pooledAddresses, cbProcess](std::map<bs::Address, AsyncClient::LedgerDelegate> ledgers) {
      auto addressesProcessed = new std::set<Address>;
      for (auto ledger : ledgers) {
         const auto &cbLedger = [this, addr = ledger.first, addressesProcessed, pooledAddresses, cbProcess]
         (std::vector<ClientClasses::LedgerEntry> entries) {
            addressesProcessed->erase(addr);
            if (!entries.empty()) {
               currentPortion_.activeAddresses.push_back((*pooledAddresses)[addr]);
            }
            if (addressesProcessed->empty()) {
               delete pooledAddresses;
               cbProcess();
            }
         };
         addressesProcessed->insert(ledger.first);
         ledger.second.getHistoryPage(0, cbLedger);
      }
   };
   std::vector<Address> addressesToScan;
   for (const auto &addr : currentPortion_.addresses) {
      addressesToScan.push_back(addr.second);
      (*pooledAddresses)[addr.second] = addr;
   }
   armoryConn_->getLedgerDelegatesForAddresses(rescanWalletId_, addressesToScan, cbDelegates);
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
   bs::Wallet::stop();
}

void hd::Leaf::SetArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   bs::Wallet::SetArmory(armory);
   hd::BlockchainScanner::setArmory(armory);
   if (armory_) {
      connect(armory_.get(), &ArmoryConnection::zeroConfReceived, this, &hd::Leaf::onZeroConfReceived, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::refresh, this, &hd::Leaf::onRefresh, Qt::QueuedConnection);
   }
}

void hd::Leaf::setRootNodes(Nodes rootNodes)
{
   rootNodes_ = rootNodes;
}

void hd::Leaf::init(const std::shared_ptr<Node> &node, const hd::Path &path, Nodes rootNodes)
{
   setRootNodes(rootNodes);

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

void hd::Leaf::onZeroConfReceived(ArmoryConnection::ReqIdType reqId)
{
   activateAddressesFromLedger(armory_->getZCentries(reqId));
}

void hd::Leaf::onRefresh(const std::vector<BinaryData> &ids)
{
   hd::BlockchainScanner::onRefresh(ids);
}

void hd::Leaf::firstInit()
{
   bs::Wallet::firstInit();

   if (activateAddressesInvoked_ || !armory_) {
      return;
   }
   const auto &cb = [this](std::vector<ClientClasses::LedgerEntry> entries) {
      activateAddressesFromLedger(entries);
   };
   activateAddressesInvoked_ = true;
   armory_->getWalletsHistory({ GetWalletId() }, cb);
}

void hd::Leaf::activateAddressesFromLedger(const std::vector<ClientClasses::LedgerEntry> &led)
{
   std::set<BinaryData> txHashes;
   for (const auto &entry : led) {
      txHashes.insert(entry.getTxHash());
   }
   const auto &cb = [this](std::vector<Tx> txs) {
      bool *activated = new bool(false);
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
         delete activated;
      };
      if (!armory_->getTXsByHash(opTxHashes, cbInputs)) {
         delete activated;
      }
   };
   armory_->getTXsByHash(txHashes, cb);
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
   const auto &decrypted = rootNodes_.decrypt(password);
   if (!decrypted) {
      return nullptr;
   }
   const auto &leafNode = decrypted->derive(path_);
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

bool hd::Leaf::getSpendableTxOutList(std::function<void(std::vector<UTXO>)>cb, uint64_t val) const
{
   const auto &cbTxOutList = [this, cb](std::vector<UTXO> txOutList) {
      std::vector<UTXO> result;
      const auto curHeight = armory_ ? armory_->topBlock() : 0;
      for (const auto &utxo : txOutList) {
         const auto &addr = bs::Address::fromUTXO(utxo);
         const auto &path = getPathForAddress(addr);
         if (path.length() < 2) {
            continue;
         }
         const uint32_t nbConf = (path.get(-2) == addrTypeExternal) ? 6 : 1;
         if (utxo.getNumConfirm(curHeight) >= nbConf) {
            result.emplace_back(utxo);
         }
      }
      cb(result);
   };
   return bs::Wallet::getSpendableTxOutList(cbTxOutList, val);
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
   RegisterWallet(armory_);
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

bool hd::Leaf::deserialize(const BinaryData &ser, Nodes rootNodes)
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
      const auto &decrypted = rootNodes.decrypt({});
      if (decrypted) {
         node = decrypted->derive(path);
      }
   }
   else {
      len = brr.get_var_int();
      BinaryData serNode = brr.get_BinaryData(len);
      node = hd::Node::deserialize(serNode);
   }
   init(node, path, rootNodes);
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
   using BinaryDataMap = std::map<BinaryData, BinaryData>;

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
      , const hd::Path &rootPath, hd::Nodes rootNodes
      , const std::map<BinaryData, hd::Path> &pathMap)
      : LeafResolver(map), password_(password), rootPath_(rootPath), rootNodes_(rootNodes), pathMap_(pathMap) {}

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey) override {
      privKey_.clear();
      const auto pathIt = pathMap_.find(pubkey);
      if (pathIt == pathMap_.end()) {
         throw std::runtime_error("no pubkey found");
      }
      const auto &decrypted = rootNodes_.decrypt(password_);
      if (!decrypted) {
         throw std::runtime_error("failed to decrypt root node[s]");
      }
      const auto &leafNode = decrypted->derive(rootPath_);
      const auto addrNode = leafNode->derive(pathIt->second);
      privKey_ = addrNode->privChainedKey();
      return privKey_;
   }

private:
   const SecureBinaryData  password_;
   const hd::Path          rootPath_;
   SecureBinaryData        privKey_;
   hd::Nodes               rootNodes_;
   const std::map<BinaryData, hd::Path>   pathMap_;
};


std::shared_ptr<ResolverFeed> hd::Leaf::GetResolver(const SecureBinaryData &password)
{
   if (isWatchingOnly()) {
      return nullptr;
   }
   return std::make_shared<LeafSigningResolver>(hashToPubKey_, password, path_, rootNodes_, pubKeyToPath_);
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

void hd::AuthLeaf::init(const std::shared_ptr<Node> &node, const hd::Path &path, Nodes rootNodes)
{
   const auto prevNode = node_;
   hd::Leaf::init(node, path, rootNodes);
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

void hd::AuthLeaf::setRootNodes(Nodes rootNodes)
{
   unchainedRootNodes_ = rootNodes;
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

   if (!unchainedRootNodes_.empty()) {
      rootNodes_ = unchainedRootNodes_.chained(userId);
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
{}

hd::CCLeaf::~CCLeaf()
{
   validationStarted_ = false;
}

void hd::CCLeaf::setData(const std::string &data)
{
   checker_ = std::make_shared<TxAddressChecker>(bs::Address(data), armory_);
}

void hd::CCLeaf::refreshInvalidUTXOs(bool ZConly)
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
      hd::Leaf::getSpendableTxOutList(cbRefresh);
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
   hd::Leaf::getSpendableZCList(cbRefreshZC);
}

void hd::CCLeaf::validationProc()
{
   validationStarted_ = true;
   if (!armory_ || (armory_->state() != ArmoryConnection::State::Ready)) {
      validationStarted_ = false;
      return;
   }
   refreshInvalidUTXOs();
   validationEnded_ = true;
   hd::Leaf::firstInit();
   emit addressAdded();

   if (!validationStarted_) {
      return;
   }

   auto addressesToCheck = new std::set<bs::Address>;
   for (const auto &addr : GetUsedAddressList()) {
      addressesToCheck->insert(addr);
   }

   const auto &cbLedgers = [this, addressesToCheck](std::map<bs::Address, AsyncClient::LedgerDelegate> ledgers) {
      for (auto ledger : ledgers) {
         if (!validationStarted_) {
            return;
         }
         const auto &cbCheck = [this, addr=ledger.first, addressesToCheck](Tx tx) {
            const auto &cbResult = [this, tx](bool contained) {
               if (!contained && tx.isInitialized()) {
                  invalidTxHash_.insert(tx.getThisHash());
               }
            };
            checker_->containsInputAddress(tx, cbResult, lotSizeInSatoshis_);

            addressesToCheck->erase(addr);
            if (addressesToCheck->empty()) {
               delete addressesToCheck;
               emit walletReset();
            }
         };
         const auto &cbHistory = [this, cbCheck](std::vector<ClientClasses::LedgerEntry> entries) {
            for (const auto &entry : entries) {
               armory_->getTxByHash(entry.getTxHash(), cbCheck);
            }
         };
         ledger.second.getHistoryPage(0, cbHistory);  //? Shouldn't we continue past the first page?
      }
   };
   armory_->getLedgerDelegatesForAddresses(GetWalletId(), GetUsedAddressList(), cbLedgers);
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
      struct Result {
         uint64_t invalidBalance = 0;
         std::set<BinaryData> txHashSet;
      };
      auto result = new Result;

      for (const auto &tx : txs) {
         const auto &itUtxo = utxoMap.find(tx.getThisHash());
         if (itUtxo == utxoMap.end()) {
            continue;
         }
         const auto &cbResult = [this, tx, itUtxo, result, cb, utxos](bool contained) {
            if (!contained) {
               invalidTx_.insert(itUtxo->second);
               invalidTxHash_.insert(tx.getThisHash());
               result->invalidBalance += itUtxo->second.getValue();
            }
            result->txHashSet.erase(tx.getThisHash());
            if (result->txHashSet.empty()) {
               balanceCorrection_ += result->invalidBalance / BTCNumericTypes::BalanceDivider;
               cb(filterUTXOs(utxos));
               delete result;
            }
         };
         result->txHashSet.insert(tx.getThisHash());
         checker_->containsInputAddress(tx, cbResult, lotSizeInSatoshis_, itUtxo->second.getValue());
      }
   };
   armory_->getTXsByHash(txHashes, cbProcess);
}

void hd::CCLeaf::firstInit()
{
   if (checker_ && !validationStarted_) {
      validationEnded_ = false;
      validationProc();
   }
}

void hd::CCLeaf::onRefresh(const std::vector<BinaryData> &ids)
{
   hd::Leaf::onRefresh(ids);
   const auto it = std::find(ids.begin(), ids.end(), GetWalletId());
   if (it == ids.end()) {
      return;
   }
   firstInit();
}

void hd::CCLeaf::onZeroConfReceived(ArmoryConnection::ReqIdType reqId)
{
   hd::Leaf::onZeroConfReceived(reqId);
   refreshInvalidUTXOs(true);
   emit addressAdded();    //?
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

bool hd::CCLeaf::getSpendableTxOutList(std::function<void(std::vector<UTXO>)>cb, uint64_t val) const
{
   if (validationStarted_ && !validationEnded_) {
      return false;
   }
   const auto &cbTxOutList = [this, cb](std::vector<UTXO> txOutList) {
      cb(filterUTXOs(txOutList));
   };
   return hd::Leaf::getSpendableTxOutList(cbTxOutList, val);
}

bool hd::CCLeaf::getSpendableZCList(std::function<void(std::vector<UTXO>)> cb) const
{
   if (validationStarted_ && !validationEnded_) {
      return false;
   }
   const auto &cbZCList = [this, cb](std::vector<UTXO> txOutList) {
      cb(filterUTXOs(txOutList));
   };
   return hd::Leaf::getSpendableZCList(cb);
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

bool hd::CCLeaf::getAddrBalance(const bs::Address &addr) const
{
   const auto &cb = [this, addr](std::vector<uint64_t> balances) {
      emit addrBalanceReceived(addr, balances);
   };
   return getAddrBalance(addr, cb);
}

bool hd::CCLeaf::getAddrBalance(const bs::Address &addr, std::function<void(std::vector<uint64_t>)> cb) const
{
   if (!lotSizeInSatoshis_ || !validationEnded_ || !bs::Wallet::isBalanceAvailable()) {
      return false;
   }
   std::vector<uint64_t> xbtBalances;
   {
      const auto itBal = addressBalanceMap_.find(addr.prefixed());
      if (itBal == addressBalanceMap_.end()) {
         return false;
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
