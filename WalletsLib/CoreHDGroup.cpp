#include "CoreHDGroup.h"
#include "CoreHDNode.h"
#include "Wallets.h"

#define LEAF_KEY     0x00002001

using namespace bs::core;


hd::Group::Group(std::shared_ptr<AssetWallet_Single> walletPtr, 
   const bs::hd::Path &path, NetworkType netType,
   const std::shared_ptr<spdlog::logger> &logger)
   : walletPtr_(walletPtr), path_(path)
   , netType_(netType), logger_(logger)
{}

std::shared_ptr<hd::Leaf> hd::Group::getLeaf(bs::hd::Path::Elem elem) const
{
   //leafs are always hardened
   elem |= 0x80000000;
   const auto itLeaf = leaves_.find(elem);
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
}

std::shared_ptr<hd::Leaf> hd::Group::getLeaf(const std::string &key) const
{
   return getLeaf(bs::hd::Path::keyToElem(key));
}

std::vector<std::shared_ptr<hd::Leaf>> hd::Group::getLeaves() const
{
   std::vector<std::shared_ptr<hd::Leaf>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::vector<std::shared_ptr<bs::core::Wallet>> hd::Group::getAllLeaves() const
{
   std::vector<std::shared_ptr<bs::core::Wallet>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(bs::hd::Path::Elem elem)
{
   //leaves are always hardened
   elem |= 0x80000000;
   if (getLeaf(elem) != nullptr) {
      return nullptr;
   }

   auto pathLeaf = path_;
   pathLeaf.append(elem);
   try
   {
      auto result = newLeaf();
      initLeaf(result, pathLeaf);
      addLeaf(result);
      return result;
   }
   catch (std::exception&)
   {
      return nullptr;
   }
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(const std::string &key)
{
   return createLeaf(bs::hd::Path::keyToElem(key));
}

bool hd::Group::addLeaf(const std::shared_ptr<hd::Leaf> &leaf)
{
   leaves_[leaf->index()] = leaf;
   needsCommit_ = true;
   return true;
}

bool hd::Group::deleteLeaf(const bs::hd::Path::Elem &elem)
{
   const auto &leaf = getLeaf(elem);
   if (leaf == nullptr) {
      return false;
   }
   leaves_.erase(elem);
   needsCommit_ = true;
   return true;
}

bool hd::Group::deleteLeaf(const std::shared_ptr<bs::core::Wallet> &wallet)
{
   bs::hd::Path::Elem elem = 0;
   bool found = false;
   for (const auto &leaf : leaves_) {
      if (leaf.second->walletId() == wallet->walletId()) {
         elem = leaf.first;
         found = true;
         break;
      }
   }
   if (!found) {
      return false;
   }
   return deleteLeaf(elem);
}

bool hd::Group::deleteLeaf(const std::string &key)
{
   return deleteLeaf(bs::hd::Path::keyToElem(key));
}

BinaryData hd::Group::serialize() const
{
   BinaryWriter bw;
   
   BinaryData path(path_.toString());
   bw.put_var_int(path.getSize());
   bw.put_BinaryData(path);

   serializeLeaves(bw);

   BinaryWriter finalBW;
   finalBW.put_var_int(bw.getSize());
   finalBW.put_BinaryData(bw.getData());
   return finalBW.getData();
}

void hd::Group::serializeLeaves(BinaryWriter &bw) const
{
   for (const auto &leaf : leaves_) {
      bw.put_uint32_t(LEAF_KEY);
      bw.put_BinaryData(leaf.second->serialize());
   }
}

std::shared_ptr<hd::Group> hd::Group::deserialize(
   std::shared_ptr<AssetWallet_Single> walletPtr, 
   BinaryDataRef key, BinaryDataRef value,
   const std::string &name, const std::string &desc,
   NetworkType netType,
   const std::shared_ptr<spdlog::logger> &logger)
{
   BinaryRefReader brrKey(key);
   auto prefix = brrKey.get_uint8_t();
   if (prefix != BS_GROUP_PREFIX) {
      return nullptr;
   }
   std::shared_ptr<hd::Group> group = nullptr;
   const bs::hd::Path emptyPath;
   const auto grpType = static_cast<bs::hd::CoinType>(brrKey.get_uint32_t());

   switch (grpType) {
   case bs::hd::CoinType::BlockSettle_Auth:
      group = std::make_shared<hd::AuthGroup>(
         walletPtr, emptyPath, netType, logger);
      break;

   case bs::hd::CoinType::Bitcoin_main:
   case bs::hd::CoinType::Bitcoin_test:
      group = std::make_shared<hd::Group>(
         walletPtr, emptyPath, netType, logger);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      group = std::make_shared<hd::CCGroup>(
         walletPtr, emptyPath, netType, logger);
      break;

   default:
      throw WalletException("unknown group type");
      break;
   }
   group->deserialize(value);
   return group;
}

std::shared_ptr<hd::Leaf> hd::Group::newLeaf() const
{
   return std::make_shared<hd::Leaf>(netType_, logger_, type());
}

void hd::Group::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path) const
{
   std::vector<unsigned> pathInt;
   for (unsigned i = 0; i < path.length(); i++)
      pathInt.push_back(path.get(i));

   //Lock the underlying armory wallet to allow accounts to derive their root from
   //the wallet's. We assume the passphrase prompt lambda is already set.
   auto lock = walletPtr_->lockDecryptedContainer();

   //setup address account
   auto accTypePtr = std::make_shared<AccountType_BIP32_Custom>();
   
   //nodes
   accTypePtr->setNodes({ hd::Leaf::addrTypeExternal, hd::Leaf::addrTypeInternal });

   //account IDs
   accTypePtr->setOuterAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal));
   accTypePtr->setInnerAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeInternal));

   //address types
   accTypePtr->setAddressTypes(getAddressTypeSet());
   accTypePtr->setDefaultAddressType(AddressEntryType_P2WPKH);

   //address lookup
   accTypePtr->setAddressLookup(/*DERIVATION_LOOKUP*/ 10);

   auto accID = walletPtr_->createBIP32Account(nullptr, pathInt, accTypePtr);
   leaf->init(walletPtr_, accID, path);
}

void hd::Group::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   auto len = brrVal.get_var_int();
   const auto strPath = brrVal.get_BinaryData(len).toBinStr();
   path_ = bs::hd::Path::fromString(strPath);

   while (brrVal.getSizeRemaining() > 4) {
      const auto keyLeaf = brrVal.get_uint32_t();
      if (keyLeaf != LEAF_KEY) {
         throw WalletException("failed to read BIP44 leaf");
      }
      len = brrVal.get_var_int();
      const auto serLeaf = brrVal.get_BinaryData(len);
      auto leafPair = hd::Leaf::deserialize(serLeaf);

      const auto leaf = newLeaf();
      leaf->init(walletPtr_, leafPair.first, leafPair.second);
      addLeaf(leaf);
   }
}

void hd::Group::shutdown()
{
   for (auto& leafPair : leaves_)
      leafPair.second->shutdown();

   walletPtr_ = nullptr;
}

void hd::Group::copyLeaves(hd::Group* from)
{
   for (auto& leafPair : from->leaves_)
   {
      auto newLeaf = std::make_shared<hd::Leaf>(
         netType_, logger_, leafPair.second->type_);
      newLeaf->init(
         walletPtr_, 
         leafPair.second->accountPtr_->getID(), 
         leafPair.second->path_);

      addLeaf(newLeaf);
   }
}

std::set<AddressEntryType> hd::Group::getAddressTypeSet(void) const
{
   return { AddressEntryType_P2PKH, AddressEntryType_P2WPKH,
      AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH)
      };
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthGroup::AuthGroup(std::shared_ptr<AssetWallet_Single> walletPtr,
   const bs::hd::Path &path, NetworkType netType,
   const std::shared_ptr<spdlog::logger>& logger)
   : Group(walletPtr, path, netType, logger)
{}

void hd::AuthGroup::setChainCode(const BinaryData &chainCode)
{
   chainCode_ = chainCode;

   if (chainCode.isNull() && tempLeaves_.empty() && !leaves_.empty()) {
      for (auto &leaf : leaves_) {
         leaf.second->setChainCode(chainCode);
      }
      tempLeaves_ = leaves_;
      for (const auto &leaf : tempLeaves_) {
         deleteLeaf(leaf.first);
      }
   }
   else if (!tempLeaves_.empty() && leaves_.empty()) {
      auto leaves = std::move(tempLeaves_);
      tempLeaves_.clear();
      for (const auto &tempLeaf : leaves) {
         tempLeaf.second->setChainCode(chainCode);
         addLeaf(tempLeaf.second);
      }
   }
}

void hd::AuthGroup::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path) const
{
   hd::Group::initLeaf(leaf, path);
   leaf->setChainCode(chainCode_);
}

std::shared_ptr<hd::Leaf> hd::AuthGroup::newLeaf() const
{
   return std::make_shared<hd::AuthLeaf>(netType_, nullptr);
}

bool hd::AuthGroup::addLeaf(const std::shared_ptr<Leaf> &leaf)
{
   if (chainCode_.isNull()) {
      tempLeaves_[leaf->index()] = leaf;
      return false;
   }
   return hd::Group::addLeaf(leaf);
}

void hd::AuthGroup::serializeLeaves(BinaryWriter &bw) const
{
   hd::Group::serializeLeaves(bw);

   for (const auto &leaf : tempLeaves_) {
      bw.put_uint32_t(LEAF_KEY);
      bw.put_BinaryData(leaf.second->serialize());
   }
}

void hd::AuthGroup::shutdown()
{
   for (auto& leafPair : tempLeaves_)
      leafPair.second->shutdown();

   hd::Group::shutdown();
}

std::set<AddressEntryType> hd::AuthGroup::getAddressTypeSet(void) const
{
   return { AddressEntryType_P2WPKH };
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Leaf> hd::CCGroup::newLeaf() const
{
   return std::make_shared<hd::CCLeaf>(netType_, logger_);
}

std::set<AddressEntryType> hd::CCGroup::getAddressTypeSet(void) const
{
   return { AddressEntryType_P2WPKH };
}
