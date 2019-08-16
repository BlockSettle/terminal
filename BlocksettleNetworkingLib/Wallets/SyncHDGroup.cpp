#include "SyncHDGroup.h"

#include "SyncWallet.h"
#include <QObject>

using namespace bs::sync;


static bs::hd::Path fixedPath(const bs::hd::Path &path)
{
   if (path.length() < 3) {
      throw std::runtime_error("invalid path length " + std::to_string(path.length()));
   }
   auto leafPath = path;
   for (size_t i = 0; i < 3; ++i) {
      leafPath.setHardened(i);
   }
   return leafPath;
}

std::shared_ptr<hd::Leaf> hd::Group::getLeaf(const bs::hd::Path &path) const
{
   const auto itLeaf = leaves_.find(fixedPath(path));
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
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

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(const bs::hd::Path &path, const std::string &walletId)
{
   const auto pathLeaf = fixedPath(path);
   const auto prevLeaf = getLeaf(pathLeaf);
   if (prevLeaf != nullptr) {
      if (walletId != prevLeaf->walletId()) {
         logger_->warn("[{}] wallet ids mismatch, new: {}, existing: {}", __func__, walletId
            , prevLeaf->walletId());
      }
      return prevLeaf;
   }
   auto result = newLeaf(walletId);
   initLeaf(result, pathLeaf);
   addLeaf(result, true);
   return result;
}

bool hd::Group::addLeaf(const std::shared_ptr<hd::Leaf> &leaf, bool signal)
{
   leaves_[leaf->path()] = leaf;
   const auto id = leaf->walletId();
   if (!id.empty()) {
      wct_->walletCreated(id);
   }
   return true;
}

bool hd::Group::deleteLeaf(const bs::hd::Path &path)
{
   const auto &leaf = getLeaf(path);
   if (leaf == nullptr) {
      return false;
   }
   const auto walletId = leaf->walletId();
   leaves_.erase(fixedPath(path));
   wct_->walletDestroyed(walletId);
   return true;
}

bool hd::Group::deleteLeaf(const std::shared_ptr<bs::sync::Wallet> &wallet)
{
   bs::hd::Path path;
   bool found = false;
   for (const auto &leaf : leaves_) {
      if (leaf.second->walletId() == wallet->walletId()) {
         path = leaf.first;
         found = true;
         break;
      }
   }
   if (!found) {
      return false;
   }
   return deleteLeaf(path);
}

std::string hd::Group::nameForType(bs::hd::CoinType ct)
{
   ct = static_cast<bs::hd::CoinType>(ct | bs::hd::hardFlag);
   switch (ct) {
   case bs::hd::CoinType::Bitcoin_main:
      return QObject::tr("XBT").toStdString();

   case bs::hd::CoinType::Bitcoin_test:
      return QObject::tr("XBT [TESTNET]").toStdString();

   case bs::hd::CoinType::BlockSettle_CC:
      return QObject::tr("Private Market Shares").toStdString();

   case bs::hd::CoinType::BlockSettle_Auth:
      return QObject::tr("Authentication").toStdString();

   case bs::hd::CoinType::BlockSettle_Settlement:
      return QObject::tr("Settlement").toStdString();
   }

   return QObject::tr("Unknown").toStdString();
}

void hd::Group::resetWCT()
{
   for (auto &leaf : leaves_) {
      leaf.second->setWCT(nullptr);
   }
}

std::shared_ptr<hd::Leaf> hd::Group::newLeaf(const std::string &walletId) const
{
   assert(bs::core::wallet::Type::Bitcoin == type());

   return std::make_shared<hd::XBTLeaf>(walletId, walletName_ + "/" + name_, desc_
      , signContainer_, logger_, extOnlyAddresses_);
}

void hd::Group::initLeaf(std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path) const
{
   leaf->setWCT(wct_);
   if (!path.length()) {
      return;
   }
   leaf->setPath(path);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthGroup::AuthGroup(const std::string &name, const std::string &desc
   , WalletSignerContainer *container, WalletCallbackTarget *wct
   , const std::shared_ptr<spdlog::logger>& logger, bool extOnlyAddresses)
   : Group(bs::hd::CoinType::BlockSettle_Auth, name
      , nameForType(bs::hd::CoinType::BlockSettle_Auth), desc
      , container, wct, logger, extOnlyAddresses)
{
   scanPortion_ = 5;
}

void hd::AuthGroup::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   for (auto &leaf : leaves_) {
      leaf.second->setUserId(userId);
   }

   if (userId.isNull() && !leaves_.empty()) {
      const auto leaves = leaves_;
      for (const auto &leaf : leaves) {
         deleteLeaf(leaf.first);
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Leaf> hd::CCGroup::newLeaf(const std::string &walletId) const
{
   return std::make_shared<hd::CCLeaf>(walletId, walletName_ + "/" + name_, desc_
      , signContainer_, logger_);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Leaf> hd::SettlementGroup::newLeaf(const std::string &walletId) const
{
   return std::make_shared<hd::SettlementLeaf>(walletId, walletName_ + "/" + name_, desc_
      , signContainer_, logger_);
}

std::shared_ptr<hd::SettlementLeaf> hd::SettlementGroup::getLeaf(const bs::Address &addr) const
{
   const auto &it = addrMap_.find(addr.unprefixed());
   if (it == addrMap_.end()) {
      return {};
   }
   return std::dynamic_pointer_cast<hd::SettlementLeaf>(hd::Group::getLeaf(it->second));
}
