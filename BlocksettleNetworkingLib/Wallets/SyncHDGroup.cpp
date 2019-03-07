#include "SyncHDGroup.h"
#include "SyncWallet.h"

using namespace bs::sync;


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

std::vector<std::shared_ptr<bs::sync::Wallet>> hd::Group::getAllLeaves() const
{
   std::vector<std::shared_ptr<bs::sync::Wallet>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(bs::hd::Path::Elem elem, const std::string &walletId)
{
   const auto prevLeaf = getLeaf(elem);
   if (prevLeaf != nullptr) {
      if (walletId != prevLeaf->walletId()) {
         logger_->warn("[{}] wallet ids mismatch, new: {}, existing: {}", __func__, walletId
            , prevLeaf->walletId());
      }
      return prevLeaf;
   }
   auto pathLeaf = path_;
   pathLeaf.append(elem, true);
   auto result = newLeaf(walletId);
   initLeaf(result, pathLeaf);
   addLeaf(result, true);
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(const std::string &key, const std::string &walletId)
{
   return createLeaf(bs::hd::Path::keyToElem(key), walletId);
}

bool hd::Group::addLeaf(const std::shared_ptr<hd::Leaf> &leaf, bool signal)
{
   connect(leaf.get(), &hd::Leaf::addressAdded, this, &hd::Group::onLeafChanged);
   leaves_[leaf->index()] = leaf;
   if (signal) {
      const auto id = QString::fromStdString(leaf->walletId());
      if (!id.isEmpty()) {
         emit leafAdded(id);
      }
      onLeafChanged();
   }
   return true;
}

bool hd::Group::deleteLeaf(const bs::hd::Path::Elem &elem)
{
   const auto &leaf = getLeaf(elem);
   if (leaf == nullptr) {
      return false;
   }
   disconnect(leaf.get(), &hd::Leaf::addressAdded, this, &hd::Group::onLeafChanged);
   const auto walletId = leaf->walletId();
   leaves_.erase(elem);
   onLeafChanged();
   emit leafDeleted(QString::fromStdString(walletId));
   return true;
}

bool hd::Group::deleteLeaf(const std::shared_ptr<bs::sync::Wallet> &wallet)
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

void hd::Group::onLeafChanged()
{
   emit changed();
}

std::string hd::Group::nameForType(bs::hd::CoinType ct)
{
   switch (ct) {
   case bs::hd::CoinType::Bitcoin_main:
      return hd::Group::tr("XBT").toStdString();

   case bs::hd::CoinType::Bitcoin_test:
      return hd::Group::tr("XBT [TESTNET]").toStdString();

   case bs::hd::CoinType::BlockSettle_CC:
      return hd::Group::tr("Private Market Shares").toStdString();

   case bs::hd::CoinType::BlockSettle_Auth:
      return hd::Group::tr("Authentication").toStdString();

   default: return hd::Group::tr("Unknown").toStdString();
   }
}

std::shared_ptr<hd::Leaf> hd::Group::newLeaf(const std::string &walletId) const
{
   return std::make_shared<hd::Leaf>(walletId, walletName_ + "/" + name_, desc_
      , signContainer_, logger_, type(), extOnlyAddresses_);
}

void hd::Group::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path) const
{
   if (!path.length()) {
      return;
   }
   leaf->init(path);
}

void hd::Group::rescanBlockchain(const hd::Group::cb_scan_notify &cb, const hd::Group::cb_scan_read_last &cbr
   , const hd::Group::cb_scan_write_last &cbw)
{
   bs::hd::Path::Elem wallet;
   for (const auto &leaf : leaves_) {
      const unsigned int startIdx = cbr ? cbr(leaf.second->walletId()) : 0;
      leaf.second->scanAddresses(startIdx, scanPortion_, cbw);
      wallet = leaf.second->index();
   }
   if (cb) {
      cb(this, leaves_.empty() ? UINT32_MAX : wallet, true);
   }
}

/*void hd::Group::copyLeaf(std::shared_ptr<hd::Group> &target, bs::hd::Path::Elem leafIndex
   , const std::shared_ptr<hd::Leaf> &leaf) const
{
   auto wallet = newLeaf(leaf->walletId());
   auto path = path_;
   path.append(leafIndex, true);
   wallet->init(path);
   leaf->copyTo(wallet);
   target->addLeaf(wallet);
}*/


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthGroup::AuthGroup(const bs::hd::Path &path, const std::string &name
   , const std::string &desc, SignContainer *container
   , const std::shared_ptr<spdlog::logger>& logger, bool extOnlyAddresses)
   : Group(path, name, nameForType(bs::hd::CoinType::BlockSettle_Auth), desc
           , container, logger, extOnlyAddresses)
{
   scanPortion_ = 5;
}

void hd::AuthGroup::setUserId(const BinaryData &userId)
{
   userId_ = userId;

   if (userId.isNull() && tempLeaves_.empty() && !leaves_.empty()) {
      for (auto &leaf : leaves_) {
         leaf.second->setUserId(userId);
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
         tempLeaf.second->setUserId(userId);
         addLeaf(tempLeaf.second, true);
      }
   }
}

void hd::AuthGroup::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path) const
{
   hd::Group::initLeaf(leaf, path);
   leaf->setUserId(userId_);
}

std::shared_ptr<hd::Leaf> hd::AuthGroup::newLeaf(const std::string &walletId) const
{
   return std::make_shared<hd::AuthLeaf>(walletId, walletName_ + "/" + name_, desc_
      , signContainer_, logger_);
}

bool hd::AuthGroup::addLeaf(const std::shared_ptr<Leaf> &leaf, bool signal)
{
   if (userId_.isNull()) {
      tempLeaves_[leaf->index()] = leaf;
      return false;
   }
   return hd::Group::addLeaf(leaf, signal);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Leaf> hd::CCGroup::newLeaf(const std::string &walletId) const
{
   return std::make_shared<hd::CCLeaf>(walletId, walletName_ + "/" + name_, desc_
      , signContainer_, logger_, extOnlyAddresses_);
}
