#include "CoreHDGroup.h"
#include "CoreHDNode.h"
#include "Wallets.h"

#define LEAF_KEY     0x00002001

using namespace bs::core;


hd::Group::Group(Nodes rootNodes, const bs::hd::Path &path, const std::string &walletName
   , const std::string &desc
   , const std::shared_ptr<spdlog::logger> &logger
   , bool extOnlyAddresses)
   : rootNodes_(rootNodes), path_(path)
   , walletName_(walletName), desc_(desc)
   , logger_(logger)
   , extOnlyAddresses_(extOnlyAddresses)
{
   netType_ = (path.get(-1) == bs::hd::CoinType::Bitcoin_test) ? NetworkType::TestNet : NetworkType::MainNet;
}

std::shared_ptr<hd::Leaf> hd::Group::getLeaf(bs::hd::Path::Elem elem) const
{
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

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(bs::hd::Path::Elem elem, const std::shared_ptr<Node> &extNode)
{
   if (getLeaf(elem) != nullptr) {
      return nullptr;
   }
   if (rootNodes_.empty() && !extNode) {
      return nullptr;
   }
   auto pathLeaf = path_;
   pathLeaf.append(elem, true);
   auto result = newLeaf();
   initLeaf(result, pathLeaf, extNode);
   addLeaf(result);
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(const std::string &key, const std::shared_ptr<Node> &extNode)
{
   return createLeaf(bs::hd::Path::keyToElem(key), extNode);
}

bool hd::Group::addLeaf(const std::shared_ptr<hd::Leaf> &leaf)
{
   leaf->setDB(dbEnv_, db_);
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

void hd::Group::setDB(const std::shared_ptr<LMDBEnv> &dbEnv, LMDB *db)
{
   dbEnv_ = dbEnv;
   db_ = db;
   for (auto leaf : leaves_) {
      leaf.second->setDB(dbEnv, db);
   }
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

std::shared_ptr<hd::Group> hd::Group::deserialize(BinaryDataRef key
                                                  , BinaryDataRef value
                                                  , Nodes rootNodes
                                                  , const std::string &name
                                                  , const std::string &desc
                                                  , const std::shared_ptr<spdlog::logger> &logger
                                                  , bool extOnlyAddresses)
{
   BinaryRefReader brrKey(key);
   auto prefix = brrKey.get_uint8_t();
   if (prefix != ASSETENTRY_PREFIX) {
      return nullptr;
   }
   std::shared_ptr<hd::Group> group = nullptr;
   const bs::hd::Path emptyPath;
   const auto grpType = static_cast<bs::hd::CoinType>(brrKey.get_uint32_t());

   switch (grpType) {
   case bs::hd::CoinType::BlockSettle_Auth:
      group = std::make_shared<hd::AuthGroup>(rootNodes, emptyPath, name, desc
                                              , logger, extOnlyAddresses);
      break;

   case bs::hd::CoinType::Bitcoin_main:
   case bs::hd::CoinType::Bitcoin_test:
      group = std::make_shared<hd::Group>(rootNodes, emptyPath, name
         , desc, logger, extOnlyAddresses);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      group = std::make_shared<hd::CCGroup>(rootNodes, emptyPath, name, desc,
                                            logger, extOnlyAddresses);
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
   return std::make_shared<hd::Leaf>(netType_, walletName_ + "/" + std::to_string(index())
      , desc_, logger_, type(), extOnlyAddresses_);
}

void hd::Group::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path, const std::shared_ptr<Node> &extNode) const
{
   if (rootNodes_.empty() && !extNode && !path.length()) {
      return;
   }
   std::shared_ptr<hd::Node> node;
   if (extNode) {
      if (path.isAbolute() && !extNode->hasPrivateKey()) {
         node = extNode;
      }
      else {
         node = extNode->derive(path);
         if (!node) {
            return;
         }
      }
   }
   else {
      const auto &decrypted = rootNodes_.decrypt({});
      if (!decrypted) {
         return;
      }
      node = decrypted->derive(path);
   }
   if (node) {
      node->clearPrivKey();
   }
   leaf->init(node, path, rootNodes_);
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
      const auto leaf = newLeaf();
      if (leaf->deserialize(serLeaf, rootNodes_)) {
         addLeaf(leaf);
      }
   }
}

std::shared_ptr<hd::Group> hd::Group::createWatchingOnly(const std::shared_ptr<Node> &extNode) const
{
   auto woGroup = createWO();
   fillWO(woGroup, extNode);
   return woGroup;
}

std::shared_ptr<hd::Group> hd::Group::createWO() const
{
   return std::make_shared<hd::Group>(Nodes(), path_, walletName_, desc_
      , logger_, extOnlyAddresses_);
}

void hd::Group::copyLeaf(std::shared_ptr<hd::Group> &target, bs::hd::Path::Elem leafIndex, const std::shared_ptr<hd::Leaf> &leaf
   , const std::shared_ptr<Node> &extNode) const
{
   auto wallet = newLeaf();
   auto path = path_;
   path.append(leafIndex, true);
   const auto node = extNode->derive(path);
   node->clearPrivKey();
   wallet->init(node, path, {});
   leaf->copyTo(wallet);
   target->addLeaf(wallet);
}

void hd::Group::fillWO(std::shared_ptr<hd::Group> &woGroup, const std::shared_ptr<Node> &extNode) const
{
   for (const auto &leaf : leaves_) {
      copyLeaf(woGroup, leaf.first, leaf.second, extNode);
   }
}

void hd::Group::updateRootNodes(Nodes rootNodes, const std::shared_ptr<hd::Node> &decrypted)
{
   rootNodes_ = rootNodes;

   for (auto &leaf : leaves_) {
      auto pathLeaf = path_;
      pathLeaf.append(leaf.first, true);
      initLeaf(leaf.second, pathLeaf, decrypted);
   }

   // Fix problem with persistence when rootNodes_ count is lowered
   needsCommit_ = true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthGroup::AuthGroup(Nodes rootNodes, const bs::hd::Path &path, const std::string &name
   , const std::string &desc, const std::shared_ptr<spdlog::logger>& logger
   , bool extOnlyAddresses)
   : Group(rootNodes, path, name, desc, logger, extOnlyAddresses)
{}

std::shared_ptr<hd::Group> hd::AuthGroup::createWO() const
{
   auto woGroup = std::make_shared<hd::AuthGroup>(Nodes(), path_, walletName_, desc_
                                                  , logger_, extOnlyAddresses_);
   woGroup->setChainCode(chainCode_);
   return woGroup;
}

void hd::AuthGroup::fillWO(std::shared_ptr<hd::Group> &woGroup, const std::shared_ptr<Node> &extNode) const
{
   hd::Group::fillWO(woGroup, extNode);
   for (const auto &leaf : tempLeaves_) {
      copyLeaf(woGroup, leaf.first, leaf.second, extNode);
   }
}

void hd::AuthGroup::setDB(const std::shared_ptr<LMDBEnv> &dbEnv, LMDB *db)
{
   hd::Group::setDB(dbEnv, db);
   for (auto leaf : tempLeaves_) {
      leaf.second->setDB(dbEnv, db);
   }
}

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

void hd::AuthGroup::updateRootNodes(Nodes rootNodes, const std::shared_ptr<hd::Node> &decrypted)
{
   hd::Group::updateRootNodes(rootNodes, decrypted);

   for (auto &leaf : tempLeaves_) {
      auto pathLeaf = path_;
      pathLeaf.append(leaf.first, true);
      initLeaf(leaf.second, pathLeaf, decrypted);
   }
}

void hd::AuthGroup::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path, const std::shared_ptr<Node> &extNode) const
{
   hd::Group::initLeaf(leaf, path, extNode);
   leaf->setChainCode(chainCode_);
}

std::shared_ptr<hd::Leaf> hd::AuthGroup::newLeaf() const
{
   return std::make_shared<hd::AuthLeaf>(netType_, walletName_ + "/" + bs::hd::Path::elemToKey(index()), desc_);
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Group> hd::CCGroup::createWO() const
{
   return std::make_shared<hd::CCGroup>(Nodes(), path_, walletName_, desc_, logger_
      , extOnlyAddresses_);
}

std::shared_ptr<hd::Leaf> hd::CCGroup::newLeaf() const
{
   return std::make_shared<hd::CCLeaf>(netType_, walletName_ + "/" + bs::hd::Path::elemToKey(index())
      , desc_, logger_, extOnlyAddresses_);
}
