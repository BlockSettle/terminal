#include "SettlementWallet.h"

#include <stdexcept>

#include <QDir>
#include <QThread>

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "FastLock.h"
#include "ScriptRecipient.h"
#include "SettlementMonitor.h"
#include "Signer.h"
#include "SystemFileUtils.h"
#include "SettlementAddressEntry.h"

#include <spdlog/spdlog.h>

//https://bitcoin.org/en/developer-guide#term-minimum-fee
constexpr uint64_t MinRelayFee = 1000;

class SettlementResolverFeed : public ResolverFeed
{
private:
   template<class payloadType>
   struct FeedItem
   {
      payloadType payload;
      std::string description;
   };

public:
   SettlementResolverFeed(const std::shared_ptr<bs::SettlementAddressEntry> &addr, const bs::KeyPair &keys) {
      CryptoECDSA crypto;
      const auto chainCode = addr->getAsset()->settlementId();
      const auto chainedPrivKey = crypto.ComputeChainedPrivateKey(keys.privKey, chainCode);
      const auto chainedPubKey = crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(keys.pubKey), chainCode));

      keys_[chainedPubKey] = FeedItem<SecureBinaryData>{ chainedPrivKey, "Private key" };

      values_[BtcUtils::getSha256(addr->getAsset()->script())] = FeedItem<BinaryData>{addr->getAsset()->script(), "Address"};
      values_[addr->getAsset()->hash()] = FeedItem<BinaryData>{addr->getAsset()->script(), "Script"};
      values_[addr->getAsset()->p2wsHash()] = FeedItem<BinaryData>{addr->getAsset()->p2wshScript(), "P2WSHScript"};
   }

   BinaryData getByVal(const BinaryData& val) override {
      auto it = values_.find(val);
      if (it == values_.end()) {
         throw std::runtime_error("Unknown value key");
      }
      return it->second.payload;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey) override {
      auto it = keys_.find(pubkey);
      if (it == keys_.end()) {
         throw std::runtime_error("Unknown pubkey");
      }
      return it->second.payload;
   }

private:
   std::map<BinaryData, struct FeedItem<BinaryData>> values_;
   std::map<BinaryData, struct FeedItem<SecureBinaryData>> keys_;
};


bs::SettlementWallet::SettlementWallet(const std::shared_ptr<spdlog::logger> &logger)
   : PlainWallet(tr("Settlement").toStdString()
                 , tr("Settlement Wallet").toStdString()
                 , logger)
{}

bs::SettlementWallet::SettlementWallet(const std::string &filename, const std::shared_ptr<spdlog::logger> &logger)
   : PlainWallet(logger)
{
   loadFromFile(filename);
}

std::string bs::SettlementWallet::getFileName(const std::string &dir) const
{
   return dir + "/" + fileNamePrefix() + "wallet.lmdb";
}

int  bs::SettlementWallet::addAddress(const std::shared_ptr<SettlementAddressEntry> &addrEntry
   , const std::shared_ptr<SettlementAssetEntry> &asset)
{
   const int id = addAddress(addrEntry->getPrefixedHash(), asset);

   auto settlementId = asset->settlementId();
   FastLock lock(lockAddressMap_);
   addrEntryByAddr_[addrEntry->getPrefixedHash()] = addrEntry;
   addressBySettlementId_[settlementId] = addrEntry;
   return id;
}

int bs::SettlementWallet::addAddress(const bs::Address &addr, const std::shared_ptr<bs::GenericAsset> &asset)
{
   const int id = bs::PlainWallet::addAddress(addr, asset);
   if (asset) {
      const auto settlAsset = std::dynamic_pointer_cast<SettlementAssetEntry>(asset);
      const auto &addrHashes = settlAsset->supportedAddrHashes();
      addrPrefixedHashes_.insert(addrHashes.begin(), addrHashes.end());
      for (const auto &hash : addrHashes) {
         assetByAddr_[hash] = asset;
      }
   }
   return id;
}

AddressEntryType bs::SettlementWallet::getAddrTypeForAddr(const BinaryData &addr)
{
   BinaryData prefixed;
   prefixed.append(NetworkConfig::getScriptHashPrefix());
   prefixed.append(addr);
   const auto itAsset = assetByAddr_.find(prefixed);
   if (itAsset != assetByAddr_.end()) {
      const auto settlAsset = std::dynamic_pointer_cast<SettlementAssetEntry>(itAsset->second);
      if (settlAsset) {
         return settlAsset->addressType();
      }
   }
   return AddressEntryType_Default;
}

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementWallet::getExistingAddress(const BinaryData &settlementId)
{
   auto address = getAddressBySettlementId(settlementId);
   if (address != nullptr) {
      createTempWalletForAsset(address->getAsset());
   }

   return address;
}

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementWallet::getAddressBySettlementId(const BinaryData &settlementId) const
{
   FastLock locker(lockAddressMap_);
   auto it = addressBySettlementId_.find(settlementId);
   if (it != addressBySettlementId_.end()) {
      return it->second;
   }

   return nullptr;
}

void bs::SettlementWallet::RefreshWallets(const std::vector<BinaryData>& ids)
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

void bs::SettlementWallet::CompleteMonitorCreations(int addressIndex
   , const std::shared_ptr<AsyncClient::BtcWallet>& addressWallet)
{
   CreateMonitorCallback createMonitorCB{};

   {
      FastLock locker{lockWalletsMap_};

      auto it = pendingMonitorCreations_.find(addressIndex);
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

bool bs::SettlementWallet::createTempWalletForAsset(const std::shared_ptr<SettlementAssetEntry>& asset)
{
   auto index = asset->id();
   const auto walletId = BtcUtils::scrAddrToBase58(asset->prefixedHash()).toBinStr();

   std::shared_ptr<AsyncClient::BtcWallet> addressWallet;

   FastLock locker{lockWalletsMap_};
   const auto regId = armory_->registerWallet(addressWallet, walletId, asset->supportedAddrHashes()
      , [](const std::string &) {}, true);

   auto completeWalletRegistration = [this, index, addressWallet]() {
      if (logger_) {
         logger_->debug("[SettlementWallet::createTempWalletForAsset] wallet registration completed");
         if (addressWallet == nullptr) {
            logger_->error("[SettlementWallet::createTempWalletForAsset] nullptr wallet");
         }
      }

      {
         FastLock locker{lockWalletsMap_};
         settlementAddressWallets_.emplace(index, addressWallet);
      }

      CompleteMonitorCreations(index, addressWallet);
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

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementWallet::newAddress(const BinaryData &settlementId, const BinaryData &buyAuthPubKey
   , const BinaryData &sellAuthPubKey, const std::string &comment)
{
   auto asset = std::make_shared<SettlementAssetEntry>(settlementId, buyAuthPubKey, sellAuthPubKey);
   auto aePtr = SettlementAssetEntry::getAddressEntry(asset);

   int id = addAddress(aePtr, asset);
   writeDB();

   if (!comment.empty()) {
      MetaData::set(std::make_shared<bs::wallet::AssetEntryComment>(id, aePtr->getPrefixedHash(), comment));
      MetaData::write(getDBEnv(), getDB());
   }

   if (armory_) {
      createTempWalletForAsset(asset);
      RegisterWallet(armory_);
   }

   emit addressAdded();
   return aePtr;
}

std::string bs::SettlementWallet::GetAddressIndex(const bs::Address &addr)
{
   const auto assetIt = assetByAddr_.find(addr.id());
   if (assetIt == assetByAddr_.end()) {
      return {};
   }
   const auto asset = std::dynamic_pointer_cast<SettlementAssetEntry>(assetIt->second);
   if (!asset) {
      return {};
   }
   return asset->settlementId().toHexStr() + "." + asset->buyAuthPubKey().toHexStr()
      + "." + asset->sellAuthPubKey().toHexStr();
}

bool bs::SettlementWallet::AddressIndexExists(const std::string &index) const
{
   const auto pos1 = index.find('.');
   if (pos1 == std::string::npos) {
      return false;
   }
   const auto &binSettlementId = BinaryData::CreateFromHex(index.substr(0, pos1));
   return (getAddressBySettlementId(binSettlementId) != nullptr);
}

bs::Address bs::SettlementWallet::CreateAddressWithIndex(const std::string &index, AddressEntryType aet, bool)
{
   if (index.empty()) {
      return {};
   }
   const auto pos1 = index.find('.');
   if (pos1 == std::string::npos) {
      return {};
   }
   const auto &binSettlementId = BinaryData::CreateFromHex(index.substr(0, pos1));
   const auto addrEntry = getAddressBySettlementId(binSettlementId);
   if (addrEntry) {
      return addrEntry->getPrefixedHash();
   }

   const auto pos2 = index.find_last_of('.');
   if (pos2 == pos1) {
      return {};
   }
   const auto buyAuthKey = index.substr(pos1 + 1, pos2 - pos1);
   const auto sellAuthKey = index.substr(pos2 + 1);
   return newAddress(binSettlementId, BinaryData::CreateFromHex(buyAuthKey)
      , BinaryData::CreateFromHex(sellAuthKey))->getPrefixedHash();
}

bool bs::SettlementWallet::containsAddress(const bs::Address &addr)
{
   if (addrPrefixedHashes_.find(addr.prefixed()) != addrPrefixedHashes_.end()) {
      return true;
   }
   if (addressHashes_.find(addr.unprefixed()) != addressHashes_.end()) {
      return true;
   }
   if (!GetAddressIndex(addr).empty()) {
      return true;
   }
   return false;
}

bool bs::SettlementWallet::GetInputFor(const std::shared_ptr<SettlementAddressEntry> &addr, std::function<void(UTXO)> cb
   , bool allowZC)
{
   const auto addressWallet = GetSettlementAddressWallet(addr->getIndex());
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
                     if(logger_ != nullptr) {
                        getLogger()->error("[bs::SettlementWallet::GetInputFor] " \
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
      catch(std::exception& e) {
         if(logger_ != nullptr) {
            logger_->error("[bs::SettlementWallet::GetInputFor] Return data " \
               "error (getSpendableTxOutListForValue) - {}", e.what());
         }
      }
   };

   addressWallet->getSpendableTxOutListForValue(UINT64_MAX, cbSpendable);
   return true;
}

uint64_t bs::SettlementWallet::GetEstimatedFeeFor(UTXO input, const bs::Address &recvAddr, float feePerByte)
{
   const auto inputAmount = input.getValue();
   if (input.txinRedeemSizeBytes_ == UINT32_MAX) {
      const auto addrEntry = getAddressEntryForAddr(input.getRecipientScrAddr());
      input.txinRedeemSizeBytes_ = (unsigned int)bs::wallet::getInputScrSize(addrEntry);
   }
   CoinSelection coinSelection([&input](uint64_t) -> std::vector<UTXO> { return { input }; }
   , std::vector<AddressBookEntry>{}, inputAmount, armory_->topBlock());

   const auto &scriptRecipient = recvAddr.getRecipient(inputAmount);
   return coinSelection.getFeeForMaxVal(scriptRecipient->getSize(), feePerByte, { input });
}

UTXO bs::SettlementWallet::GetInputFromTX(const std::shared_ptr<SettlementAddressEntry> &addr, const BinaryData &payinHash, const double amount) const
{
   const uint64_t value = amount * BTCNumericTypes::BalanceDivider;
   const uint32_t txHeight = UINT32_MAX;
   const auto hash = BtcUtils::getSha256(addr->getScript());

   return UTXO(value, txHeight, 0, 0, payinHash, BtcUtils::getP2WSHOutputScript(hash));
}

bs::wallet::TXSignRequest bs::SettlementWallet::CreatePayoutTXRequest(const UTXO &input, const bs::Address &recvAddr
   , float feePerByte)
{
   bs::wallet::TXSignRequest txReq;
   txReq.inputs.push_back(input);
   uint64_t fee = GetEstimatedFeeFor(input, recvAddr, feePerByte);

   if (fee < MinRelayFee) {
      fee = MinRelayFee;
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

BinaryData bs::SettlementWallet::SignPayoutTXRequest(const bs::wallet::TXSignRequest &req, const KeyPair &keys
   , const BinaryData &settlementId, const BinaryData &buyAuthKey, const BinaryData &sellAuthKey)
{
   auto addr = getAddressBySettlementId(settlementId);
   if (!addr) {
      addr = newAddress(settlementId, buyAuthKey, sellAuthKey);
   }
   auto resolverFeed = std::make_shared<SettlementResolverFeed>(addr, keys);

   Signer signer;
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);

   if ((req.inputs.size() == 1) && (req.recipients.size() == 1)) {
      auto spender = std::make_shared<ScriptSpender>(req.inputs[0], resolverFeed);
      signer.addSpender(spender);
      signer.addRecipient(req.recipients[0]);
   }
   else if (!req.prevStates.empty()) {
      for (const auto &prevState : req.prevStates) {
         signer.deserializeState(prevState);
      }
   }

   if (req.populateUTXOs) {
      for (const auto &utxo : req.inputs) {
         signer.populateUtxo(utxo);
      }
   }
   signer.setFeed(resolverFeed);

   signer.sign();
   if (!signer.verify()) {
      throw std::logic_error("signer failed to verify");
   }
   return signer.serialize();
}


std::shared_ptr<AddressEntry> bs::SettlementWallet::getAddressEntryForAddr(const BinaryData &addr)
{
   const auto &itAddrEntry = addrEntryByAddr_.find(addr);
   if (itAddrEntry == addrEntryByAddr_.end()) {
      return nullptr;
   }
   return itAddrEntry->second;
}

SecureBinaryData bs::SettlementWallet::GetPublicKeyFor(const bs::Address &addr)
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
}

bool bs::SettlementWallet::createMonitorQtSignals(const std::shared_ptr<SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::function<void (const std::shared_ptr<SettlementMonitorQtSignals>&)>& userCB)
{
   auto createMonitorCB = [this, addr, logger, userCB](const std::shared_ptr<AsyncClient::BtcWallet>& addressWallet)
   {
      userCB(std::make_shared<bs::SettlementMonitorQtSignals>(addressWallet, armory_, addr, logger));
   };

   return createMonitorCommon(addr, logger, createMonitorCB);
}

bool bs::SettlementWallet::createMonitorCb(const std::shared_ptr<SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::function<void (const std::shared_ptr<SettlementMonitorCb>&)>& userCB)
{
   auto createMonitorCB = [this, addr, logger, userCB](const std::shared_ptr<AsyncClient::BtcWallet>& addressWallet)
   {
      userCB(std::make_shared<bs::SettlementMonitorCb>(addressWallet, armory_, addr, logger));
   };

   return createMonitorCommon(addr, logger, createMonitorCB);
}

bool bs::SettlementWallet::createMonitorCommon(const std::shared_ptr<SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , const CreateMonitorCallback& createMonitorCB)
{
   const auto addressIndex = addr->getIndex();

   std::shared_ptr<AsyncClient::BtcWallet> addressWallet = nullptr;

   {
      FastLock locker{lockWalletsMap_};

      auto it = settlementAddressWallets_.find(addressIndex);
      if (settlementAddressWallets_.end() != it) {
         addressWallet = it->second;
      } else {
         // sanity check. only single monitor for address allowed
         if (pendingMonitorCreations_.find(addressIndex) != pendingMonitorCreations_.end()) {
            // use logger passed as parameter
            logger->error("[SettlementWallet::createMonitorCommon] multiple monitors on same address are not allowed");
            return false;
         }

         pendingMonitorCreations_.emplace(addressIndex, createMonitorCB);
      }
   }

   // wallet was registered, so we could create monitor
   if (addressWallet != nullptr) {
      createMonitorCB(addressWallet);
   }

   return true;
}

std::shared_ptr<AsyncClient::BtcWallet> bs::SettlementWallet::GetSettlementAddressWallet(const int addressIndex) const
{
   FastLock locker{lockWalletsMap_};

   auto it = settlementAddressWallets_.find(addressIndex);
   if (settlementAddressWallets_.end() != it) {
      return it->second;
   }

   return nullptr;
}
