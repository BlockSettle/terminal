#include "SyncSettlementWallet.h"
#include <stdexcept>
#include <QDir>
#include <QThread>
#include <spdlog/spdlog.h>

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "FastLock.h"
#include "ScriptRecipient.h"
#include "SettlementMonitor.h"
#include "SignContainer.h"

using namespace bs::sync;


SettlementWallet::SettlementWallet(const std::string &walletId, const std::string &name, const std::string &desc
   , SignContainer *container,const std::shared_ptr<spdlog::logger> &logger)
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

bool SettlementWallet::getInputFor(const bs::Address &addr, std::function<void(UTXO)> cb
   , bool allowZC)
{
   const auto addressWallet = getSettlementAddressWallet(addr);
   if (addressWallet == nullptr) {
      return false;
   }

   const auto &cbSpendable = [this, cb, allowZC, addressWallet]
                             (ReturnMessage<std::vector<UTXO>> inputs)->void {
      try {
         auto inUTXOs = inputs.get();
         if (inUTXOs.empty()) {
            if (allowZC) {
               const auto &cbZC = [this, cb]
                                  (ReturnMessage<std::vector<UTXO>> zcs)->void {
                  try {
                     auto inZCUTXOs = zcs.get();
                     if (inZCUTXOs.size() == 1) {
                        cb(inZCUTXOs[0]);
                     }
                  }
                  catch(std::exception& e) {
                     if (logger_ != nullptr) {
                        logger_->error("[bs::SettlementWallet::GetInputFor] " \
                           "Return data error (getSpendableZCList) - {}",
                           e.what());
                     }
                  }
               };
               addressWallet->getSpendableZCList(cbZC);
            }
         }
         else if (inUTXOs.size() == 1) {
            cb(inUTXOs[0]);
         }
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::SettlementWallet::GetInputFor] Return data " \
               "error (getSpendableTxOutListForValue) - {}", e.what());
         }
      }
   };

   addressWallet->getSpendableTxOutListForValue(UINT64_MAX, cbSpendable);
   return true;
}

uint64_t SettlementWallet::getEstimatedFeeFor(UTXO input, const bs::Address &recvAddr, float feePerByte)
{
   const auto inputAmount = input.getValue();
   if (input.txinRedeemSizeBytes_ == UINT32_MAX) {
      const bs::Address scrAddr(input.getRecipientScrAddr());
      input.txinRedeemSizeBytes_ = (unsigned int)scrAddr.getInputSize();
   }
   CoinSelection coinSelection([&input](uint64_t) -> std::vector<UTXO> { return { input }; }
   , std::vector<AddressBookEntry>{}, inputAmount, armory_->topBlock());

   const auto &scriptRecipient = recvAddr.getRecipient(inputAmount);
   return coinSelection.getFeeForMaxVal(scriptRecipient->getSize(), feePerByte, { input });
}

UTXO SettlementWallet::getInputFromTX(const bs::Address &addr, const BinaryData &payinHash, const double amount) const
{
   const auto addrEntry = getAddressEntryForAddr(addr);
   if (!addrEntry) {
      return {};
   }
   const uint64_t value = amount * BTCNumericTypes::BalanceDivider;
   const uint32_t txHeight = UINT32_MAX;
   const auto hash = BtcUtils::getSha256(addrEntry->getScript());

   return UTXO(value, txHeight, 0, 0, payinHash, BtcUtils::getP2WSHOutputScript(hash));
}

bs::core::wallet::TXSignRequest SettlementWallet::createPayoutTXRequest(const UTXO &input, const bs::Address &recvAddr
   , float feePerByte)
{
   bs::core::wallet::TXSignRequest txReq;
   txReq.inputs.push_back(input);
   uint64_t fee = getEstimatedFeeFor(input, recvAddr, feePerByte);

   if (fee < bs::sync::wallet::kMinRelayFee) {
      fee = bs::sync::wallet::kMinRelayFee;
   }

   uint64_t value = input.getValue();
   if (value < fee) {
      value = 0;
   } else {
      value = value - fee;
   }

   txReq.fee = fee;
   txReq.recipients.emplace_back(recvAddr.getRecipient(value));
   return txReq;
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
