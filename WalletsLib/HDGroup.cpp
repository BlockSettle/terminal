#include "HDGroup.h"
#include "HDNode.h"
#include "Wallets.h"

#define LEAF_KEY     0x00002001

using namespace bs;


std::shared_ptr<hd::Leaf> hd::Group::getLeaf(hd::Path::Elem elem) const
{
   const auto itLeaf = leaves_.find(elem);
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
}

std::shared_ptr<hd::Leaf> hd::Group::getLeaf(const std::string &key) const
{
   return getLeaf(hd::Path::keyToElem(key));
}

std::vector<std::shared_ptr<bs::hd::Leaf>> hd::Group::getLeaves() const
{
   std::vector<std::shared_ptr<bs::hd::Leaf>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::vector<std::shared_ptr<bs::Wallet>> hd::Group::getAllLeaves() const
{
   std::vector<std::shared_ptr<bs::Wallet>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(Path::Elem elem, const std::shared_ptr<Node> &extNode)
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
   addLeaf(result, true);
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(const std::string &key, const std::shared_ptr<Node> &extNode)
{
   return createLeaf(hd::Path::keyToElem(key), extNode);
}

bool hd::Group::addLeaf(const std::shared_ptr<hd::Leaf> &leaf, bool signal)
{
   connect(leaf.get(), &hd::Leaf::addressAdded, this, &hd::Group::onLeafChanged);
   leaf->setDB(dbEnv_, db_);
   leaves_[leaf->index()] = leaf;
   if (signal) {
      const auto id = QString::fromStdString(leaf->GetWalletId());
      if (!id.isEmpty()) {
         emit leafAdded(id);
      }
      onLeafChanged();
   }
   needsCommit_ = true;
   return true;
}

bool hd::Group::deleteLeaf(const Path::Elem &elem)
{
   const auto &leaf = getLeaf(elem);
   if (leaf == nullptr) {
      return false;
   }
   disconnect(leaf.get(), &hd::Leaf::addressAdded, this, &hd::Group::onLeafChanged);
   const auto walletId = leaf->GetWalletId();
   leaves_.erase(elem);
   onLeafChanged();
   emit leafDeleted(QString::fromStdString(walletId));
   return true;
}

bool hd::Group::deleteLeaf(const std::shared_ptr<bs::Wallet> &wallet)
{
   Path::Elem elem = 0;
   bool found = false;
   for (const auto &leaf : leaves_) {
      if (leaf.second->GetWalletId() == wallet->GetWalletId()) {
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
   return deleteLeaf(hd::Path::keyToElem(key));
}

void hd::Group::onLeafChanged()
{
   needsCommit_ = true;
   emit changed();
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

std::shared_ptr<hd::Group> hd::Group::deserialize(BinaryDataRef key, BinaryDataRef value, Nodes rootNodes
   , const std::string &name, const std::string &desc, bool extOnlyAddresses)
{
   BinaryRefReader brrKey(key);
   auto prefix = brrKey.get_uint8_t();
   if (prefix != ASSETENTRY_PREFIX) {
      return nullptr;
   }
   std::shared_ptr<hd::Group> group = nullptr;
   const hd::Path emptyPath;
   const auto grpType = static_cast<CoinType>(brrKey.get_uint32_t());

   switch (grpType) {
   case CoinType::BlockSettle_Auth:
      group = std::make_shared<hd::AuthGroup>(rootNodes, emptyPath, name, desc, extOnlyAddresses);
      break;

   case CoinType::Bitcoin_main:
   case CoinType::Bitcoin_test:
      group = std::make_shared<hd::Group>(rootNodes, emptyPath, name, nameForType(grpType), desc, extOnlyAddresses);
      break;

   case CoinType::BlockSettle_CC:
      group = std::make_shared<hd::CCGroup>(rootNodes, emptyPath, name, desc, extOnlyAddresses);
      break;

   default:
      throw WalletException("unknown group type");
      break;
   }
   group->deserialize(value);
   return group;
}

std::string hd::Group::nameForType(CoinType ct)
{
   switch (ct) {
   case CoinType::Bitcoin_main:
      return hd::Group::tr("XBT").toStdString();

   case CoinType::Bitcoin_test:
      return hd::Group::tr("XBT [TESTNET]").toStdString();

   case CoinType::BlockSettle_CC:
      return hd::Group::tr("Private Market Shares").toStdString();

   case CoinType::BlockSettle_Auth:
      return hd::Group::tr("Authentication").toStdString();

   default: return hd::Group::tr("Unknown").toStdString();
   }
}

std::shared_ptr<hd::Leaf> hd::Group::newLeaf() const
{
   return std::make_shared<hd::Leaf>(walletName_ + "/" + name_, desc_, getType(), extOnlyAddresses_);
}

void hd::Group::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const Path &path, const std::shared_ptr<Node> &extNode) const
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
   path_ = Path::fromString(strPath);

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

void hd::Group::rescanBlockchain(const hd::Group::cb_scan_notify &cb, const hd::Group::cb_scan_read_last &cbr
   , const hd::Group::cb_scan_write_last &cbw)
{
   hd::Path::Elem wallet;
   for (const auto &leaf : leaves_) {
      const unsigned int startIdx = cbr ? cbr(leaf.second->GetWalletId()) : 0;
      leaf.second->scanAddresses(startIdx, scanPortion_, cbw);
      wallet = leaf.second->index();
   }
   if (cb) {
      cb(this, leaves_.empty() ? UINT32_MAX : wallet, true);
   }
}

std::shared_ptr<hd::Group> hd::Group::CreateWatchingOnly(const std::shared_ptr<Node> &extNode) const
{
   auto woGroup = CreateWO();
   FillWO(woGroup, extNode);
   return woGroup;
}

std::shared_ptr<hd::Group> hd::Group::CreateWO() const
{
   return std::make_shared<hd::Group>(Nodes(), path_, walletName_, name_, desc_, extOnlyAddresses_);
}

void hd::Group::copyLeaf(std::shared_ptr<hd::Group> &target, hd::Path::Elem leafIndex, const std::shared_ptr<hd::Leaf> &leaf
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

void hd::Group::FillWO(std::shared_ptr<hd::Group> &woGroup, const std::shared_ptr<Node> &extNode) const
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
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthGroup::AuthGroup(Nodes rootNodes, const Path &path, const std::string &name
   , const std::string &desc, bool extOnlyAddresses)
   : Group(rootNodes, path, name, nameForType(CoinType::BlockSettle_Auth), desc, extOnlyAddresses)
{
   scanPortion_ = 5;
}

std::shared_ptr<hd::Group> hd::AuthGroup::CreateWO() const
{
   auto woGroup = std::make_shared<hd::AuthGroup>(Nodes(), path_, name_, desc_, extOnlyAddresses_);
   woGroup->setUserID(userId_);
   return woGroup;
}

void hd::AuthGroup::FillWO(std::shared_ptr<hd::Group> &woGroup, const std::shared_ptr<Node> &extNode) const
{
   hd::Group::FillWO(woGroup, extNode);
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

void hd::AuthGroup::setUserID(const BinaryData &userId)
{
   userId_ = userId;

   if (tempLeaves_.empty() && !leaves_.empty()) {
      for (auto &leaf : leaves_) {
         leaf.second->SetUserID(userId);
      }
   }
   else {
      auto leaves = std::move(tempLeaves_);
      for (const auto &tempLeaf : leaves) {
         tempLeaf.second->SetUserID(userId);
         if (addLeaf(tempLeaf.second)) {
            emit leafAdded(QString::fromStdString(tempLeaf.second->GetWalletId()));
         }
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

void hd::AuthGroup::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const Path &path, const std::shared_ptr<Node> &extNode) const
{
   hd::Group::initLeaf(leaf, path, extNode);
   leaf->SetUserID(userId_);
}

std::shared_ptr<hd::Leaf> hd::AuthGroup::newLeaf() const
{
   return std::make_shared<hd::AuthLeaf>(walletName_ + "/" + name_, desc_);
}

bool hd::AuthGroup::addLeaf(const std::shared_ptr<Leaf> &leaf, bool signal)
{
   if (userId_.isNull()) {
      tempLeaves_[leaf->index()] = leaf;
      return false;
   }
   return hd::Group::addLeaf(leaf, signal);
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

std::shared_ptr<hd::Group> hd::CCGroup::CreateWO() const
{
   return std::make_shared<hd::CCGroup>(Nodes(), path_, name_, desc_, extOnlyAddresses_);
}

std::shared_ptr<hd::Leaf> hd::CCGroup::newLeaf() const
{
   return std::make_shared<hd::CCLeaf>(walletName_ + "/" + name_, desc_, extOnlyAddresses_);
}
