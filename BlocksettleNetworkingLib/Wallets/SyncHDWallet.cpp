#include "SyncHDWallet.h"
#include <QtConcurrent/QtConcurrentRun>
#include "SignContainer.h"
#include "SyncWallet.h"


#define LOG(logger, method, ...) \
if ((logger)) { \
   logger->method(__VA_ARGS__); \
}

using namespace bs::sync;

hd::Wallet::Wallet(NetworkType netType, const std::string &walletId, const std::string &name
   , const std::string &desc, const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), walletId_(walletId), name_(name), desc_(desc), netType_(netType)
   , logger_(logger)
{}

hd::Wallet::Wallet(NetworkType netType, const std::string &walletId, const std::string &name
   , const std::string &desc, SignContainer *container
   , const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), walletId_(walletId), name_(name), desc_(desc), netType_(netType)
   , signContainer_(container), logger_(logger)
{}

hd::Wallet::~Wallet() = default;

void hd::Wallet::synchronize(const std::function<void()> &cbDone)
{
   if (!signContainer_) {
      return;
   }
   const auto &cbProcess = [this, cbDone](HDWalletData data) {
      for (const auto &grpData : data.groups) {
         auto group = createGroup(grpData.type);
         if (!group) {
            LOG(logger_, error, "[hd::Wallet::synchronize] failed to create group {}", (uint32_t)grpData.type);
            continue;
         }
         for (const auto &leafData : grpData.leaves) {
            auto leaf = group->createLeaf(leafData.index, leafData.id);
            if (!leaf) {
               LOG(logger_, error, "[hd::Wallet::synchronize] failed to create leaf {}/{} with id {}"
                  , (uint32_t)grpData.type, leafData.index, leafData.id);
               continue;
            }
         }
      }
      const auto leaves = getLeaves();
      auto leafIds = std::make_shared<std::set<std::string>>();
      for (const auto &leaf : leaves) {
         leafIds->insert(leaf->walletId());
      }
      for (const auto &leaf : leaves) {
         const auto &cbLeafDone = [this, leaf, leafIds, cbDone] {
            leafIds->erase(leaf->walletId());
            if (encryptionTypes_.empty()) {
               encryptionTypes_ = leaf->encryptionTypes();
               encryptionKeys_ = leaf->encryptionKeys();
               encryptionRank_ = leaf->encryptionRank();
               emit metaDataChanged();
            }
            if (leafIds->empty() && cbDone) {
               cbDone();
            }
         };
         leaf->synchronize(cbLeafDone);
      }
   };
   signContainer_->syncHDWallet(walletId(), cbProcess);
}

std::string hd::Wallet::walletId() const
{
   return walletId_;
}

std::vector<std::shared_ptr<hd::Group>> hd::Wallet::getGroups() const
{
   std::vector<std::shared_ptr<hd::Group>> result;
   result.reserve(groups_.size());
   {
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         result.emplace_back(group.second);
      }
   }
   return result;
}

size_t hd::Wallet::getNumLeaves() const
{
   size_t result = 0;
   {
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         result += group.second->getNumLeaves();
      }
   }
   return result;
}

std::vector<std::shared_ptr<bs::sync::Wallet>> hd::Wallet::getLeaves() const
{
   const auto nbLeaves = getNumLeaves();
   if (leaves_.size() != nbLeaves) {
      leaves_.clear();
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         const auto &groupLeaves = group.second->getAllLeaves();
         for (const auto &leaf : groupLeaves) {
            leaves_[leaf->walletId()] = leaf;
         }
      }
   }

   std::vector<std::shared_ptr<bs::sync::Wallet>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<bs::sync::Wallet> hd::Wallet::getLeaf(const std::string &id) const
{
   const auto &itLeaf = leaves_.find(id);
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
}

std::shared_ptr<hd::Group> hd::Wallet::createGroup(bs::hd::CoinType ct)
{
   std::shared_ptr<hd::Group> result;
   result = getGroup(ct);
   if (result) {
      return result;
   }

   const bs::hd::Path path({ bs::hd::purpose, ct });
   switch (ct) {
   case bs::hd::CoinType::BlockSettle_Auth:
      result = std::make_shared<hd::AuthGroup>(path, name_, desc_, signContainer_
         , logger_, extOnlyAddresses_);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      result = std::make_shared<hd::CCGroup>(path, name_, desc_,signContainer_
         , logger_, extOnlyAddresses_);
      break;

   default:
      result = std::make_shared<hd::Group>(path, name_, hd::Group::nameForType(ct)
         , desc_, signContainer_, logger_, extOnlyAddresses_);
      break;
   }
   addGroup(result);
   return result;
}

void hd::Wallet::addGroup(const std::shared_ptr<hd::Group> &group)
{
   connect(group.get(), &hd::Group::changed, this, &hd::Wallet::onGroupChanged);
   connect(group.get(), &hd::Group::leafAdded, this, &hd::Wallet::onLeafAdded);
   connect(group.get(), &hd::Group::leafDeleted, this, &hd::Wallet::onLeafDeleted);
   if (!userId_.isNull()) {
      group->setUserId(userId_);
   }

   QMutexLocker lock(&mtxGroups_);
   groups_[group->index()] = group;
}

std::shared_ptr<hd::Group> hd::Wallet::getGroup(bs::hd::CoinType ct) const
{
   QMutexLocker lock(&mtxGroups_);
   const auto itGroup = groups_.find(static_cast<bs::hd::Path::Elem>(ct));
   if (itGroup == groups_.end()) {
      return nullptr;
   }
   return itGroup->second;
}

void hd::Wallet::onGroupChanged()
{
//   updatePersistence();
}

void hd::Wallet::onLeafAdded(QString id)
{
   for (const auto &leaf : getLeaves()) {
      if ((leaf->walletId() == id.toStdString()) && armory_) {
         leaf->setArmory(armory_);
      }
   }
   emit leafAdded(id);
}

void hd::Wallet::onLeafDeleted(QString id)
{
   getLeaves();
   emit leafDeleted(id);
}

void hd::Wallet::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   std::vector<std::shared_ptr<hd::Group>> groups;
   groups.reserve(groups_.size());
   {
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         groups.push_back(group.second);
      }
   }
   for (const auto &group : groups) {
      group->setUserId(userId);
   }
}

void hd::Wallet::setArmory(const std::shared_ptr<ArmoryObject> &armory)
{
   armory_ = armory;
   for (const auto &leaf : getLeaves()) {
      leaf->setArmory(armory);
   }
}

void hd::Wallet::registerWallet(const std::shared_ptr<ArmoryObject> &armory, bool asNew)
{
   for (const auto &leaf : getLeaves()) {
      leaf->registerWallet(armory, asNew);
   }
}

bool hd::Wallet::startRescan(const hd::Wallet::cb_scan_notify &cb, const cb_scan_read_last &cbr
   , const cb_scan_write_last &cbw)
{
   {
      QMutexLocker lock(&mtxGroups_);
      if (!scannedLeaves_.empty()) {
         return false;
      }
   }
   QtConcurrent::run(this, &hd::Wallet::rescanBlockchain, cb, cbr, cbw);
   return true;
}

bool hd::Wallet::deleteRemotely()
{
   if (!signContainer_) {
      return false;
   }
   return (signContainer_->DeleteHDRoot(walletId_) > 0);
}

void hd::Wallet::rescanBlockchain(const hd::Wallet::cb_scan_notify &cb, const cb_scan_read_last &cbr
   , const cb_scan_write_last &cbw)
{
   QMutexLocker lock(&mtxGroups_);
   for (const auto &group : groups_) {
      group.second->rescanBlockchain(cb, cbr, cbw);
      for (const auto &leaf : group.second->getLeaves()) {
         scannedLeaves_.insert(leaf->walletId());
         connect(leaf.get(), &hd::Leaf::scanComplete, this, &hd::Wallet::onScanComplete);
      }
   }
}

void hd::Wallet::onScanComplete(const std::string &leafId)
{
   QMutexLocker lock(&mtxGroups_);
   scannedLeaves_.erase(leafId);
   if (scannedLeaves_.empty()) {
      emit scanComplete(walletId());
   }
}

bool hd::Wallet::isPrimary() const
{
   if ((getGroup(bs::hd::CoinType::BlockSettle_Auth) != nullptr)
      && (getGroup(getXBTGroupType()) != nullptr)) {
      return true;
   }
   return false;
}

std::vector<bs::wallet::EncryptionType> hd::Wallet::encryptionTypes() const
{
   return encryptionTypes_;
}

std::vector<SecureBinaryData> hd::Wallet::encryptionKeys() const
{
   return encryptionKeys_;
}

bs::wallet::KeyRank hd::Wallet::encryptionRank() const
{
   return encryptionRank_;
}
