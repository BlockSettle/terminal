#include "SyncHDWallet.h"

#include "CheckRecipSigner.h"
#include "SyncWallet.h"
#include "WalletSignerContainer.h"

#include <QtConcurrent/QtConcurrentRun>

#define LOG(logger, method, ...) \
if ((logger)) { \
   logger->method(__VA_ARGS__); \
}

using namespace bs::sync;

hd::Wallet::Wallet(const std::string &walletId, const std::string &name
   , const std::string &desc, bool isOffline, WalletSignerContainer *container
   , const std::shared_ptr<spdlog::logger> &logger)
   : walletId_(walletId), name_(name), desc_(desc)
   , signContainer_(container), logger_(logger), isOffline_(isOffline)
{
   netType_ = getNetworkType();
}

hd::Wallet::~Wallet()
{
   for (auto &group : groups_) {
      group.second->resetWCT();
   }
}

void hd::Wallet::synchronize(const std::function<void()> &cbDone)
{
   if (!signContainer_)
      return;

   const auto &cbProcess = [this, cbDone](HDWalletData data)
   {
      for (const auto &grpData : data.groups) {
         auto group = getGroup(grpData.type);
         if (!group) {
            group = createGroup(grpData.type, grpData.extOnly);
            if (grpData.type == bs::hd::CoinType::BlockSettle_Auth &&
               grpData.salt.getSize() == 32) {
               auto authGroupPtr = 
                  std::dynamic_pointer_cast<hd::AuthGroup>(group);
               if (authGroupPtr == nullptr)
                  throw std::runtime_error("unexpected sync group type");

               authGroupPtr->setUserId(grpData.salt);
            }
         }
         if (!group) {
            LOG(logger_, error, "[hd::Wallet::synchronize] failed to create group {}", (uint32_t)grpData.type);
            continue;
         }

         for (const auto &leafData : grpData.leaves) {
            auto leaf = group->getLeaf(leafData.index);
            if (!leaf) {
               leaf = group->createLeaf(leafData.index, leafData.id);
            }
            if (!leaf) {
               LOG(logger_, error, "[hd::Wallet::synchronize] failed to create leaf {}/{} with id {}"
                  , (uint32_t)grpData.type, leafData.index, leafData.id);
               continue;
            }
            if (grpData.type == bs::hd::CoinType::BlockSettle_Settlement) {
               if (leafData.extraData.isNull()) {
                  throw std::runtime_error("no extra data for settlement leaf " + leafData.id);
               }
               const auto settlGroup = std::dynamic_pointer_cast<hd::SettlementGroup>(group);
               if (!settlGroup) {
                  throw std::runtime_error("invalid settlement group type");
               }
               settlGroup->addMap(leafData.extraData, leafData.index);
            }
         }
      }

      const auto leaves = getLeaves();
      auto leafIds = std::make_shared<std::set<std::string>>();
      for (const auto &leaf : leaves)
         leafIds->insert(leaf->walletId());

      for (const auto &leaf : leaves) {
         const auto &cbLeafDone = [leafIds, cbDone, id=leaf->walletId()] 
         {
            leafIds->erase(id);
            if (leafIds->empty() && cbDone)
               cbDone();
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

std::shared_ptr<hd::Group> hd::Wallet::createGroup(bs::hd::CoinType ct, bool isExtOnly)
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
         , this, logger_, isExtOnly);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      result = std::make_shared<hd::CCGroup>(path, name_, desc_, signContainer_
         , this, logger_, isExtOnly);
      break;

   case bs::hd::CoinType::BlockSettle_Settlement:
      result = std::make_shared<hd::SettlementGroup>(
         path, name_, desc_, signContainer_, this, logger_);
      break;

   default:
      result = std::make_shared<hd::Group>(path, name_, hd::Group::nameForType(ct)
         , desc_, signContainer_, this, logger_, isExtOnly);
      break;
   }
   addGroup(result);
   return result;
}

void hd::Wallet::addGroup(const std::shared_ptr<hd::Group> &group)
{
   if (!userId_.isNull()) {
      group->setUserId(userId_);
   }

   groups_[group->index()] = group;
}

std::shared_ptr<hd::Group> hd::Wallet::getGroup(bs::hd::CoinType ct) const
{
   const auto itGroup = groups_.find(static_cast<bs::hd::Path::Elem>(ct));
   if (itGroup == groups_.end()) {
      return nullptr;
   }
   return itGroup->second;
}

void hd::Wallet::walletCreated(const std::string &walletId)
{
   for (const auto &leaf : getLeaves()) {
      if ((leaf->walletId() == walletId) && armory_) {
         leaf->setArmory(armory_);
      }
   }

   if (wct_) {
      wct_->walletCreated(walletId);
   }
}

void hd::Wallet::walletDestroyed(const std::string &walletId)
{
   getLeaves();
   if (wct_) {
      wct_->walletDestroyed(walletId);
   }
}

void hd::Wallet::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   std::vector<std::shared_ptr<hd::Group>> groups;
   groups.reserve(groups_.size());
   {
      for (const auto &group : groups_) {
         groups.push_back(group.second);
      }
   }
   for (const auto &group : groups) {
      group->setUserId(userId);
   }
}

void hd::Wallet::setArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   armory_ = armory;

   for (const auto &leaf : getLeaves()) {
      leaf->setArmory(armory);
   }
}

std::vector<std::string> hd::Wallet::registerWallet(
   const std::shared_ptr<ArmoryConnection> &armory, bool asNew)
{
   std::vector<std::string> result;
   for (const auto &leaf : getLeaves()) 
   {
      //settlement leaves are not registered
      if (leaf->type() == bs::core::wallet::Type::Settlement)
         continue;

      auto&& regIDs = leaf->registerWallet(armory, asNew);
      result.insert(result.end(), regIDs.begin(), regIDs.end());
   }

   return result;
}

std::vector<std::string> hd::Wallet::setUnconfirmedTargets()
{
   std::vector<std::string> result;
   for (const auto &leaf : getLeaves()) 
   {
      auto hdLeafPtr = std::dynamic_pointer_cast<hd::Leaf>(leaf);
      if (hdLeafPtr == nullptr)
         continue;

      auto&& regIDs = hdLeafPtr->setUnconfirmedTarget();
      result.insert(result.end(), regIDs.begin(), regIDs.end());
   }

   return result;
}

void hd::Wallet::scan(const std::function<void(bs::sync::SyncState)> &cb)
{
   auto stateMap = std::make_shared<std::map<std::string, bs::sync::SyncState>>();
   const auto &leaves = getLeaves();
   const auto nbLeaves = leaves.size();
   for (const auto &leaf : leaves) {
      const auto &cbScanLeaf = [this, leaf, nbLeaves, stateMap, cb](bs::sync::SyncState state) {
         (*stateMap)[leaf->walletId()] = state;
         if (stateMap->size() == nbLeaves) {
            bs::sync::SyncState hdState = bs::sync::SyncState::Failure;
            for (const auto &st : *stateMap) {
               if (st.second < hdState) {
                  hdState = st.second;
               }
            }
            leaf->synchronize([this, leaf, cb, hdState] {
               if (wct_) {
                  wct_->addressAdded(leaf->walletId());
               }
               if (cb) {
                  cb(hdState);
               }
            });
         }
      };
      logger_->debug("[{}] scanning leaf {}...", __func__, leaf->walletId());
      leaf->scan(cbScanLeaf);
   }
}

void hd::Wallet::startRescan()
{
   const auto &cbScanned = [this](bs::sync::SyncState state) {
//      synchronize([]{});
      if (wct_) {
         wct_->scanComplete(walletId());
      }
   };
   scan(cbScanned);
}

bool hd::Wallet::deleteRemotely()
{
   if (!signContainer_) {
      return false;
   }
   return (signContainer_->DeleteHDRoot(walletId_) > 0);
}

bool hd::Wallet::isPrimary() const
{
   if (isOffline()) {
      return false;
   }

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

void hd::Wallet::merge(const Wallet& rhs)
{
   //rudimentary implementation, flesh it out on the go
   for (auto& leafPair : rhs.leaves_)
   {
      auto iter = leaves_.find(leafPair.first);
      if (iter == leaves_.end())
      {
         leaves_.insert(leafPair);
         continue;
      }

      auto& leafPtr = iter->second;
      leafPtr->merge(leafPair.second);
   }
}

void hd::Wallet::setWCT(WalletCallbackTarget *wct)
{
   wct_ = wct;
   for (const auto &leaf : getLeaves()) {
      leaf->setWCT(wct);
   }
}

void hd::Wallet::getSettlementPayinAddress(const SecureBinaryData &settlementID
   , const SecureBinaryData &counterPartyPubKey, const bs::sync::Wallet::CbAddress &cb
   , bool isMyKeyFirst) const
{
   if (!signContainer_) {
      if (cb)
         cb({});
      return;
   }
   const auto &cbWrap = [cb](bool, const bs::Address &addr) {
      if (cb)
         cb(addr);
   };
   signContainer_->getSettlementPayinAddress(walletId(), { settlementID
      , counterPartyPubKey, isMyKeyFirst }, cbWrap);
}
