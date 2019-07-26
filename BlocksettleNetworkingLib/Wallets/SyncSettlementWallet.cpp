#include "SyncSettlementWallet.h"
#include <stdexcept>
#include <QDir>
#include <QThread>
#include <spdlog/spdlog.h>

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "FastLock.h"
#include "ScriptRecipient.h"
#include "SettlementMonitor.h"
#include "WalletSignerContainer.h"

using namespace bs::sync;


SettlementWallet::SettlementWallet(const std::string &walletId, const std::string &name, const std::string &desc
   , WalletSignerContainer *container,const std::shared_ptr<spdlog::logger> &logger)
   : PlainWallet(walletId, name, desc, container, logger)
{}

bs::Address SettlementWallet::getExistingAddress(const BinaryData &settlementId)
{
   auto address = getAddressBySettlementId(settlementId);
   if (!address.isNull()) {
      createTempWalletForAddress(address);
   }
   return address;
}

bs::Address SettlementWallet::getAddressBySettlementId(const BinaryData &settlementId) const
{
   FastLock locker(lockAddressMap_);
   const auto &it = addrBySettlementId_.find(settlementId);
   if (it != addrBySettlementId_.end()) {
      return it->second;
   }

   return {};
}

void SettlementWallet::refreshWallets(const std::vector<BinaryData>& ids)
{
   bool pendingRegistrationFound = false;

   for (const auto& i : ids) {
      const std::string id = i.toBinStr();

      std::function<void()> completeWalletRegistration{};

      {
         FastLock locker{lockWalletsMap_};
         auto it = pendingWalletRegistrations_.find(id);
         if (it != pendingWalletRegistrations_.end()) {
            completeWalletRegistration = it->second;
            pendingWalletRegistrations_.erase(it);
         }
      }

      if (completeWalletRegistration) {
         pendingRegistrationFound = true;
         completeWalletRegistration();
      }
   }
   if (!pendingRegistrationFound) {
      if (logger_) {
         logger_->debug("[SettlementWallet::RefreshWallets] no pending registrations found");
      }
   }
}

void SettlementWallet::completeMonitorCreations(const bs::Address &addr
   , const std::shared_ptr<AsyncClient::BtcWallet>& addressWallet)
{
   CreateMonitorCallback createMonitorCB{};

   {
      FastLock locker{lockWalletsMap_};

      auto it = pendingMonitorCreations_.find(addr);
      if (it == pendingMonitorCreations_.end()) {
         if (logger_) {
            logger_->debug("[SettlementWallet::CompleteMonitorCreations] no pending monitors for registered wallet");
         }
         return;
      }

      createMonitorCB = it->second;
   }

   createMonitorCB(addressWallet);
}

bool SettlementWallet::createTempWalletForAddress(const bs::Address& addr)
{
   const auto walletId = addr.display();
   const auto settlAddr = getAddressEntryForAddr(addr);
   if (!settlAddr) {
      return false;
   }

   const auto addressWallet = armory_->instantiateWallet(walletId);

   FastLock locker{lockWalletsMap_};
   const auto regId = armory_->registerWallet(addressWallet, walletId, walletId
      , settlAddr->supportedAddrHashes(), [](const std::string &) {}, true);

   auto completeWalletRegistration = [this, addr, addressWallet]() {
      if (logger_) {
         logger_->debug("[SettlementWallet::createTempWalletForAsset] wallet registration completed");
         if (addressWallet == nullptr) {
            logger_->error("[SettlementWallet::createTempWalletForAsset] nullptr wallet");
         }
      }

      {
         FastLock locker{lockWalletsMap_};
         settlementAddressWallets_.emplace(addr, addressWallet);
      }

      completeMonitorCreations(addr, addressWallet);
   };

   if (regId.empty()) {
      if (logger_ != nullptr) {
         logger_->error("[SettlementWallet::createTempWalletForAsset] failed to start wallet registration in armory");
      }
      return false;
   }

   pendingWalletRegistrations_.emplace(regId, completeWalletRegistration);

   return true;
}

void SettlementWallet::newAddress(const CbAddress &cb, const BinaryData &settlementId
   , const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey, const std::string &comment)
{
   if (!signContainer_) {
      cb({});
      return;
   }
   const auto &cbSettlAddr = [this, cb, settlementId, comment](const bs::Address &addr) {
      setAddressComment(addr, comment);
      {
         FastLock locker{ lockAddressMap_ };
         addrBySettlementId_[settlementId] = addr;
         settlementIdByAddr_[addr] = settlementId;
      }

      if (armory_) {
         createTempWalletForAddress(addr);
         registerWallet(armory_);
      }

      cb(addr);
      if (wct_) {
         wct_->addressAdded(walletId());
      }
   };
   const auto index = settlementId.toHexStr() + "." + buyAuthPubKey.toHexStr() + "."
      + sellAuthPubKey.toHexStr();
//!   signContainer_->syncNewAddress(walletId(), index, AddressEntryType_Default, cbSettlAddr);
   // signContainer_->syncAddressBatch(...);
}

std::string SettlementWallet::getAddressIndex(const bs::Address &addr)
{
   const auto &itSettlId = settlementIdByAddr_.find(addr);
   if (itSettlId == settlementIdByAddr_.end()) {
      return {};
   }
   return itSettlId->second.toHexStr();
}

bool SettlementWallet::addressIndexExists(const std::string &index) const
{
   return !getAddressBySettlementId(BinaryData::CreateFromHex(index)).isNull();
}

bool SettlementWallet::containsAddress(const bs::Address &addr)
{
   if (addrPrefixedHashes_.find(addr.prefixed()) != addrPrefixedHashes_.end()) {
      return true;
   }
   if (!getAddressIndex(addr).empty()) {
      return true;
   }
   return false;
}

std::shared_ptr<bs::SettlementAddress> SettlementWallet::getAddressEntryForAddr(const bs::Address &addr) const
{
   const auto &itAddrEntry = addrEntryByAddr_.find(addr);
   if (itAddrEntry == addrEntryByAddr_.end()) {
      return nullptr;
   }
   return itAddrEntry->second;
}

/*SecureBinaryData bs::SettlementWallet::GetPublicKeyFor(const bs::Address &addr)
{
   if (addr.isNull()) {
      return {};
   }
   const auto &itAsset = assetByAddr_.find(addr);
   if (itAsset == assetByAddr_.end()) {
      return {};
   }
   const auto settlAsset = std::dynamic_pointer_cast<SettlementAssetEntry>(itAsset->second);
   return settlAsset ? settlAsset->settlementId() : SecureBinaryData{};
}

bs::KeyPair bs::SettlementWallet::GetKeyPairFor(const bs::Address &addr, const SecureBinaryData &password)
{
   return {};
}*/

bool SettlementWallet::createMonitorQtSignals(const bs::Address &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::function<void (const std::shared_ptr<bs::SettlementMonitorQtSignals>&)>& userCB)
{
   auto createMonitorCB = [this, addr, logger, userCB](const std::shared_ptr<AsyncClient::BtcWallet>& addressWallet)
   {
      const auto addrEntry = getAddressEntryForAddr(addr);
      userCB(std::make_shared<bs::SettlementMonitorQtSignals>(addressWallet, armory_, addrEntry, addr, logger));
   };

   return createMonitorCommon(addr, logger, createMonitorCB);
}

bool SettlementWallet::createMonitorCb(const bs::Address &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::function<void (const std::shared_ptr<bs::SettlementMonitorCb>&)>& userCB)
{
   auto createMonitorCB = [this, addr, logger, userCB](const std::shared_ptr<AsyncClient::BtcWallet>& addressWallet)
   {
      const auto addrEntry = getAddressEntryForAddr(addr);
      userCB(std::make_shared<bs::SettlementMonitorCb>(addressWallet, armory_, addrEntry, addr, logger));
   };

   return createMonitorCommon(addr, logger, createMonitorCB);
}

bool SettlementWallet::createMonitorCommon(const bs::Address &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , const CreateMonitorCallback& createMonitorCB)
{
   auto addressWallet = getSettlementAddressWallet(addr);
   if (addressWallet) {
      createMonitorCB(addressWallet);
   }
   else {
      FastLock locker{lockWalletsMap_};
      // sanity check. only single monitor for address allowed
      if (pendingMonitorCreations_.find(addr) != pendingMonitorCreations_.end()) {
         // use logger passed as parameter
         logger->error("[SettlementWallet::createMonitorCommon] multiple monitors on same address are not allowed");
         return false;
      }

      pendingMonitorCreations_.emplace(addr, createMonitorCB);
   }
   return true;
}

std::shared_ptr<AsyncClient::BtcWallet> SettlementWallet::getSettlementAddressWallet(const bs::Address &addr) const
{
   FastLock locker{lockWalletsMap_};

   auto it = settlementAddressWallets_.find(addr);
   if (settlementAddressWallets_.end() != it) {
      return it->second;
   }
   return nullptr;
}
