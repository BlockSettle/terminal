#include <unordered_map>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "CoreHDLeaf.h"
#include "CoreHDNode.h"
#include "Wallets.h"

#define ADDR_KEY     0x00002002

using namespace bs::core;

hd::Leaf::Leaf(NetworkType netType, const std::string &name, const std::string &desc, const std::shared_ptr<spdlog::logger> &logger
   , wallet::Type type, bool extOnlyAddresses)
   : Wallet(netType, logger)
   , type_(type), name_(name), desc_(desc), isExtOnly_(extOnlyAddresses)
{ }

void hd::Leaf::setRootNodes(Nodes rootNodes)
{
   rootNodes_ = rootNodes;
}

void hd::Leaf::init(const std::shared_ptr<Node> &node, const bs::hd::Path &path, Nodes rootNodes)
{
   setRootNodes(rootNodes);

   if (path != path_) {
      path_ = path;
      suffix_.clear();
      suffix_ = bs::hd::Path::elemToKey(index());
      walletName_ = name_ + "/" + suffix_;
   }

   if (node) {
      if (node != node_) {
         node_ = node;
      }
   }
   else {
      reset();
   }
}

bool hd::Leaf::copyTo(std::shared_ptr<hd::Leaf> &leaf) const
{
   for (const auto &addr : addressMap_) {
      const auto &address = std::get<0>(addr.second);
      const auto newAddr = leaf->createAddress(std::get<2>(addr.second), addr.first, address.getType());
      const auto comment = getAddressComment(address);
      if (!comment.empty()) {
         if (!leaf->setAddressComment(newAddr, comment)) {
         }
      }
   }
   for (const auto &addr : tempAddresses_) {
      leaf->createAddress(std::get<0>(addr.second), addr.first, std::get<1>(addr.second));
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
   addressHashes_.clear();
   hashToPubKey_.clear();
   pubKeyToPath_.clear();
   addressPool_.clear();
   poolByAddr_.clear();
}

std::string hd::Leaf::walletId() const
{
   if (walletId_.empty() && node_) {
      walletId_ = node_->getId();
   }
   return walletId_;
}

std::string hd::Leaf::description() const
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

std::vector<bs::Address> hd::Leaf::getPooledAddressList() const
{
   std::vector<bs::Address> result;
   for (const auto &addr : poolByAddr_) {
      result.push_back(addr.first);
   }
   return result;
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
      return extAddresses_[rand() % extAddresses_.size()];
   }
   else {
      if (!lastIntIdx_) {
         return getNewChangeAddress(aet);
      }
      else {
         return intAddresses_[rand() % intAddresses_.size()];
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

void hd::Leaf::addAddress(const bs::Address &addr, const BinaryData &pubChainedKey, const bs::hd::Path &path)
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
   const auto index = addressIndex(addr);
   if (index == UINT32_MAX) {
      return nullptr;
   }
   const auto itTuple = addressMap_.find(index);
   if (itTuple == addressMap_.end()) {
      return nullptr;
   }
   return std::get<1>(itTuple->second);
}

SecureBinaryData hd::Leaf::getPublicKeyFor(const bs::Address &addr)
{
   const auto node = getNodeForAddr(addr);
   if (node == nullptr) {
      return BinaryData();
   }
   return node->pubCompressedKey();
}

SecureBinaryData hd::Leaf::getPubChainedKeyFor(const bs::Address &addr)
{
   const auto node = getNodeForAddr(addr);
   if (node == nullptr) {
      return BinaryData();
   }
   return node->pubChainedKey();
}

std::shared_ptr<hd::Node> hd::Leaf::getPrivNodeFor(const bs::Address &addr, const SecureBinaryData &password)
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

KeyPair hd::Leaf::getKeyPairFor(const bs::Address &addr, const SecureBinaryData &password)
{
   const auto &node = getPrivNodeFor(addr, password);
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
   return createAddress(addrPath, lastIntIdx_ + lastExtIdx_, aet);
}

bs::Address hd::Leaf::createAddress(const bs::hd::Path &path, bs::hd::Path::Elem index
   , AddressEntryType aet, bool persistent)
{
   const bool isInternal = (path.get(-2) == addrTypeInternal);
   if (isInternal && isExtOnly_) {
      return {};
   }
   bs::Address result;

   std::shared_ptr<hd::Node> addrNode;
   AddressEntryType addrType = aet;
   if (aet == AddressEntryType_Default) {
      addrType = defaultAET_;
   }
   const auto addrPoolIt = addressPool_.find({ path, addrType });
   if (addrPoolIt != addressPool_.end()) {
      result = addrPoolIt->second;
      if (persistent) {
         addressPool_.erase(addrPoolIt->first);
         poolByAddr_.erase(result);
         if (node_) {
            addrNode = node_->derive(path, true);
         }
      }
   }
   else {
      if (result.isNull()) {
         if (node_) {
            addrNode = node_->derive(path, true);
         }
         if (addrNode == nullptr) {
            return {};
         }
         result = bs::Address::fromPubKey(addrNode->pubChainedKey(), addrType);
      }
   }

   if (!persistent || (addrToIndex_.find(result.unprefixed()) != addrToIndex_.end())) {
      return result;
   }

   if (isInternal) {
      intAddresses_.push_back(result);
   }
   else {
      extAddresses_.push_back(result);
   }

   usedAddresses_.push_back(result);
   addrToIndex_[result.unprefixed()] = index;
   addressHashes_.insert(result.unprefixed());

   if (addrNode) {
      const auto complementaryAddrType = (aet == AddressEntryType_P2SH) ? AddressEntryType_P2WPKH : AddressEntryType_P2SH;
      const auto &complementaryAddr = Address::fromPubKey(addrNode->pubChainedKey(), complementaryAddrType);

      addressMap_[index] = AddressTuple(result, addrNode, path);
      addrToIndex_[complementaryAddr.unprefixed()] = index;
      addressHashes_.insert(complementaryAddr.unprefixed());
      addAddress(result, addrNode->pubChainedKey(), path);
      addAddress(complementaryAddr, addrNode->pubChainedKey(), path);
   }
   return result;
}

bs::Address hd::Leaf::newAddress(const bs::hd::Path &path, AddressEntryType aet)
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

std::vector<hd::Leaf::PooledAddress> hd::Leaf::generateAddresses(
   bs::hd::Path::Elem prefix, bs::hd::Path::Elem start, size_t nb, AddressEntryType aet)
{
   std::vector<PooledAddress> result;
   result.reserve(nb);
   for (bs::hd::Path::Elem i = start; i < start + nb; i++) {
      bs::hd::Path addrPath({ prefix, i });
      const auto &addr = newAddress(addrPath, aet);
      if (!addr.isNull()) {
         result.emplace_back(PooledAddress({ addrPath, aet }, addr));
      }
   }
   return result;
}

void hd::Leaf::topUpAddressPool(size_t nbIntAddresses, size_t nbExtAddresses)
{
   const size_t nbPoolInt = nbIntAddresses ? 0 : getLastAddrPoolIndex(addrTypeInternal) - lastIntIdx_ + 1;
   const size_t nbPoolExt = nbExtAddresses ? 0 : getLastAddrPoolIndex(addrTypeExternal) - lastExtIdx_ + 1;
   nbIntAddresses = std::max<size_t>(nbIntAddresses, intAddressPoolSize_);
   nbExtAddresses = std::max<size_t>(nbExtAddresses, extAddressPoolSize_);

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
}

bs::hd::Path::Elem hd::Leaf::getAddressIndexForAddr(const BinaryData &addr) const
{
   bs::Address p2pk(addr, AddressEntryType_P2PKH);
   bs::Address p2sh(addr, AddressEntryType_P2SH);
   bs::hd::Path::Elem index = UINT32_MAX;
   for (const auto &bd : { p2pk.unprefixed(), p2sh.unprefixed() }) {
      const auto itIndex = addrToIndex_.find(bd);
      if (itIndex != addrToIndex_.end()) {
         index = itIndex->second;
         break;
      }
   }
   return index;
}

bs::hd::Path::Elem hd::Leaf::addressIndex(const bs::Address &addr) const
{
   const auto itIndex = addrToIndex_.find(addr.unprefixed());
   if (itIndex == addrToIndex_.end()) {
      return UINT32_MAX;
   }
   return itIndex->second;
}

bs::hd::Path hd::Leaf::getPathForAddress(const bs::Address &addr) const
{
   const auto index = addressIndex(addr);
   if (index == UINT32_MAX) {
      const auto itAddr = poolByAddr_.find(addr);
      if (itAddr == poolByAddr_.end()) {
         return {};
      }
      return itAddr->second.path;
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
      if (std::get<2>(addr.second) == path) {
         return true;
      }
   }
   return false;
}

bs::Address hd::Leaf::createAddressWithIndex(const std::string &index, bool persistent, AddressEntryType aet)
{
   const auto addr = createAddressWithPath(bs::hd::Path::fromString(index), persistent, aet);
   return addr;
}

bs::Address hd::Leaf::createAddressWithPath(const bs::hd::Path &path, bool persistent, AddressEntryType aet)
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
         const auto address = std::get<0>(addr.second);
         if ((aet != AddressEntryType_Default) && (aet == address.getType())) {
            return address;
         }
      }
   }
   auto &lastIndex = (path.get(-2) == addrTypeInternal) ? lastIntIdx_ : lastExtIdx_;
   const auto prevLastIndex = lastIndex;
   const auto addrIndex = path.get(-1);
   const int nbAddresses = addrIndex - lastIndex;
   if (nbAddresses > 0) {
      for (const auto &addr : generateAddresses(path.get(-2), lastIndex, nbAddresses, aet)) {
         lastIndex++;
         createAddress(addr.first.path, lastIntIdx_ + lastExtIdx_, aet, persistent);
      }
   }
   lastIndex++;
   const auto result = createAddress(addrPath, lastIntIdx_ + lastExtIdx_, aet, persistent);
   if (!persistent) {
      lastIndex = prevLastIndex;
   }

   return result;
}

bs::hd::Path::Elem hd::Leaf::getLastAddrPoolIndex(bs::hd::Path::Elem addrType) const
{
   bs::hd::Path::Elem result = 0;
   for (const auto &addr : addressPool_) {
      const auto &path = addr.first.path;
      if (path.get(-2) == addrType) {
         result = std::max(result, path.get(-1));
      }
   }
   if (!result) {
      result = (addrType == addrTypeInternal) ? lastIntIdx_ - 1 : lastExtIdx_ - 1;
   }
   return result;
}

void hd::Leaf::serializeAddr(BinaryWriter &bw, bs::hd::Path::Elem index, AddressEntryType aet, const bs::hd::Path &path)
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
   auto path = bs::hd::Path::fromString(strPath);
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
      path = bs::hd::Path::fromString(strPath);
      bs::hd::Path addrPath;
      if (path.length() <= 2) {
         addrPath = path;
      }
      else {
         addrPath.append(path.get(-2));
         addrPath.append(path.get(-1));
      }
      const auto &addr = createAddress(addrPath, index, addrType);
      if (!addr.isNull()) {
         const auto actualIdx = addrPath.get(-1);
         if (addrPath.get(0) == addrTypeExternal) {
            lastExtIdx_ = std::max<uint32_t>(lastExtIdx_, actualIdx + 1);
         }
         else {
            lastIntIdx_ = std::max<uint32_t>(lastIntIdx_, actualIdx + 1);
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

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData&) override {
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
      , const bs::hd::Path &rootPath, hd::Nodes rootNodes
      , const std::map<BinaryData, bs::hd::Path> &pathMap)
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
   const bs::hd::Path      rootPath_;
   SecureBinaryData        privKey_;
   hd::Nodes               rootNodes_;
   const std::map<BinaryData, bs::hd::Path>  pathMap_;
};


std::shared_ptr<ResolverFeed> hd::Leaf::getResolver(const SecureBinaryData &password)
{
   if (isWatchingOnly()) {
      return nullptr;
   }
   return std::make_shared<LeafSigningResolver>(hashToPubKey_, password, path_, rootNodes_, pubKeyToPath_);
}

std::shared_ptr<ResolverFeed> hd::Leaf::getPublicKeyResolver()
{
   return std::make_shared<LeafResolver>(hashToPubKey_);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(NetworkType netType, const std::string &name, const std::string &desc
   , const std::shared_ptr<spdlog::logger> &logger)
   : Leaf(netType, name, desc, logger, wallet::Type::Authentication)
{
   intAddressPoolSize_ = 0;
   extAddressPoolSize_ = 0;
}

void hd::AuthLeaf::init(const std::shared_ptr<Node> &node, const bs::hd::Path &path, Nodes rootNodes)
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
}

void hd::AuthLeaf::setRootNodes(Nodes rootNodes)
{
   unchainedRootNodes_ = rootNodes;
}

bs::Address hd::AuthLeaf::createAddress(const bs::hd::Path &path, bs::hd::Path::Elem index
   , AddressEntryType aet, bool persistent)
{
   if (chainCode_.isNull()) {
      tempAddresses_[index] = { path, aet };
      return {};
   }
   return hd::Leaf::createAddress(path, index, aet, persistent);
}

void hd::AuthLeaf::setChainCode(const BinaryData &chainCode)
{
   chainCode_ = chainCode;
   if (chainCode.isNull()) {
      reset();
      return;
   }

   if (!unchainedRootNodes_.empty()) {
      rootNodes_ = unchainedRootNodes_.chained(chainCode);
   }
   if (unchainedNode_) {
      node_ = std::make_shared<hd::ChainedNode>(*unchainedNode_, chainCode);

      for (const auto &addr : tempAddresses_) {
         const auto &path = addr.second.first;
         createAddress(path, addr.first, addr.second.second);
         lastExtIdx_ = std::max<uint32_t>(lastExtIdx_, path.get(-1) + 1);
      }
      const auto poolAddresses = generateAddresses(addrTypeExternal, lastExtIdx_, 5, AddressEntryType_P2WPKH);
      for (const auto &addr : poolAddresses) {
         addressPool_[addr.first] = addr.second;
         poolByAddr_[addr.second] = addr.first;
      }
   }
}
