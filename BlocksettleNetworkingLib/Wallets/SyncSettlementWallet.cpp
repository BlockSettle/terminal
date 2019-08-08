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

using namespace bs::sync;


SettlementWallet::SettlementWallet(const std::shared_ptr<spdlog::logger> &logger)
   : PlainWallet(CryptoPRNG::generateRandom(4).toHexStr(), "settlement", {}, nullptr, logger)
{}

bs::Address SettlementWallet::getExistingAddress(const BinaryData &settlementId)
{
   const auto sd = getSettlDataBySettlementId(settlementId);
   if (!sd.address.isNull()) {
      createTempWalletForAddress(sd.address, sd.buyPubKey, sd.sellPubKey);
   }
   return sd.address;
}

SettlementWallet::SettlementData SettlementWallet::getSettlDataBySettlementId(const BinaryData &settlementId) const
{
   FastLock locker(lockAddressMap_);
   const auto &it = addrData_.find(settlementId);
   if (it != addrData_.end()) {
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
   , const BinaryData &buyPubKey, const BinaryData &sellPubKey
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

   createMonitorCB(addressWallet, addr, buyPubKey, sellPubKey);
}

bool SettlementWallet::createTempWalletForAddress(const bs::Address& addr
   , const BinaryData &buyPubKey, const BinaryData &sellPubKey)
{
   const auto walletId = addr.display();

   const auto addressWallet = armory_->instantiateWallet(walletId);

   FastLock locker{lockWalletsMap_};
   const auto regId = armory_->registerWallet(addressWallet, walletId, walletId
      , { addr.id() }, [](const std::string &) {}, true);

   auto completeWalletRegistration = [this, addr, buyPubKey, sellPubKey, addressWallet]() {
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

      completeMonitorCreations(addr, buyPubKey, sellPubKey, addressWallet);
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

bs::Address SettlementWallet::createSettlementAddr(const BinaryData &settlementId
   , const BinaryData &buyPubKey, const BinaryData &sellPubKey)
{
   auto&& buySaltedKey = CryptoECDSA::PubKeyScalarMultiply(buyPubKey, settlementId);
   auto&& sellSaltedKey = CryptoECDSA::PubKeyScalarMultiply(sellPubKey, settlementId);

   auto buyAsset = std::make_shared<AssetEntry_Single>(0, BinaryData()
      , buySaltedKey, nullptr);
   auto sellAsset = std::make_shared<AssetEntry_Single>(0, BinaryData()
      , sellSaltedKey, nullptr);

   //create ms asset
   std::map<BinaryData, std::shared_ptr<AssetEntry>> assetMap;

   assetMap.insert(std::make_pair(READHEX("00"), buyAsset));
   assetMap.insert(std::make_pair(READHEX("01"), sellAsset));

   auto assetMs = std::make_shared<AssetEntry_Multisig>(
      0, BinaryData(), assetMap, 1, 2);

   //create ms address
   auto addrMs = std::make_shared<AddressEntry_Multisig>(assetMs, true);

   //nest it
   auto addrP2wsh = std::make_shared<AddressEntry_P2WSH>(addrMs);

   return bs::Address(addrP2wsh->getPrefixedHash());

}

void SettlementWallet::newAddress(const CbAddress &cb, const BinaryData &settlementId
   , const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey)
{
   const auto addr = createSettlementAddr(settlementId, buyAuthPubKey, sellAuthPubKey);
   {
      FastLock locker{ lockAddressMap_ };
      addrData_[settlementId] = { addr, buyAuthPubKey, sellAuthPubKey };
      settlementIdByAddr_[addr] = settlementId;
   }

   if (armory_) {
      createTempWalletForAddress(addr, buyAuthPubKey, sellAuthPubKey);
      registerWallet(armory_);
   }

   cb(addr);
   if (wct_) {
      wct_->addressAdded(walletId());
   }
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
   return !getSettlDataBySettlementId(BinaryData::CreateFromHex(index)).address.isNull();
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

bool SettlementWallet::createMonitorCb(const BinaryData &settlementId
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::function<void (const std::shared_ptr<bs::SettlementMonitorCb>&)>& userCB)
{
   auto createMonitorCB = [this, logger, userCB]
      (const std::shared_ptr<AsyncClient::BtcWallet> &addressWallet, const bs::Address &settlAddr
         , const BinaryData &buyPubKey, const BinaryData &sellPubKey)
   {
      userCB(std::make_shared<bs::SettlementMonitorCb>(armory_, logger
         , settlAddr, buyPubKey, sellPubKey, [] {}, addressWallet));
   };

   return createMonitorCommon(settlementId, logger, createMonitorCB);
}

bool SettlementWallet::createMonitorCommon(const BinaryData &settlementId
   , const std::shared_ptr<spdlog::logger>& logger
   , const CreateMonitorCallback& createMonitorCB)
{
   std::shared_ptr<AsyncClient::BtcWallet> addressWallet;
   const SettlementData sd = getSettlDataBySettlementId(settlementId);
   if (!sd.address.isNull()) {
      addressWallet = getSettlementAddressWallet(sd.address);
   }
   if (addressWallet) {
      createMonitorCB(addressWallet, sd.address, sd.buyPubKey, sd.sellPubKey);
   }
   else {
      FastLock locker{lockWalletsMap_};
      // sanity check. only single monitor for address allowed
      if (pendingMonitorCreations_.find(settlementId) != pendingMonitorCreations_.end()) {
         // use logger passed as parameter
         logger->error("[SettlementWallet::createMonitorCommon] multiple monitors on same address are not allowed");
         return false;
      }

      pendingMonitorCreations_.emplace(settlementId, createMonitorCB);
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
