#include <stdexcept>
//!#include <QDir>
#include <spdlog/spdlog.h>

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "FastLock.h"
#include "ScriptRecipient.h"
#include "Signer.h"
#include "SystemFileUtils.h"
#include "CoreSettlementWallet.h"

using namespace bs::core;

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
   SettlementResolverFeed(const std::shared_ptr<SettlementAddressEntry> &addr, const KeyPair &keys) {
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


SettlementWallet::SettlementWallet(NetworkType netType, const std::shared_ptr<spdlog::logger> &logger)
   : PlainWallet(netType, "Settlement", "Settlement Wallet", logger)
{}

SettlementWallet::SettlementWallet(NetworkType netType, const std::string &filename, const std::shared_ptr<spdlog::logger> &logger)
   : PlainWallet(netType, logger)
{
   loadFromFile(filename);
}

std::string SettlementWallet::getFileName(const std::string &dir) const
{
   return dir + "/" + fileNamePrefix() + "wallet.lmdb";
}

int  SettlementWallet::addAddress(const std::shared_ptr<SettlementAddressEntry> &addrEntry
   , const std::shared_ptr<SettlementAssetEntry> &asset)
{
   const int id = addAddress(addrEntry->getPrefixedHash(), asset);

   auto settlementId = asset->settlementId();
   FastLock lock(lockAddressMap_);
   addrEntryByAddr_[addrEntry->getPrefixedHash()] = addrEntry;
   addressBySettlementId_[settlementId] = addrEntry;
   return id;
}

int SettlementWallet::addAddress(const bs::Address &addr, const std::shared_ptr<GenericAsset> &asset)
{
   const int id = PlainWallet::addAddress(addr, asset);
   if (asset) {
      const auto settlAsset = std::dynamic_pointer_cast<SettlementAssetEntry>(asset);
      const auto &addrHashes = settlAsset->supportedAddrHashes();
      for (const auto &hash : addrHashes) {
         assetByAddr_[hash] = asset;
      }
   }
   return id;
}

std::shared_ptr<SettlementAddressEntry> SettlementWallet::getExistingAddress(const BinaryData &settlementId)
{
   return getAddressBySettlementId(settlementId);
}

std::shared_ptr<SettlementAddressEntry> SettlementWallet::getAddressBySettlementId(const BinaryData &settlementId) const
{
   FastLock locker(lockAddressMap_);
   auto it = addressBySettlementId_.find(settlementId);
   if (it != addressBySettlementId_.end()) {
      return it->second;
   }

   return nullptr;
}

std::shared_ptr<SettlementAddressEntry> SettlementWallet::newAddress(const BinaryData &settlementId, const BinaryData &buyAuthPubKey
   , const BinaryData &sellAuthPubKey, const std::string &comment, bool persistent)
{
   auto asset = std::make_shared<SettlementAssetEntry>(settlementId, buyAuthPubKey, sellAuthPubKey);
   auto aePtr = SettlementAssetEntry::getAddressEntry(asset);

   int id = addAddress(aePtr, asset);
   if (persistent) {
      writeDB();
   }

   if (persistent && !comment.empty()) {
      MetaData::set(std::make_shared<wallet::AssetEntryComment>(id, aePtr->getPrefixedHash(), comment));
      MetaData::write(getDBEnv(), getDB());
   }

   return aePtr;
}

std::string SettlementWallet::getAddressIndex(const bs::Address &addr)
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

bool SettlementWallet::addressIndexExists(const std::string &index) const
{
   const auto pos1 = index.find('.');
   if (pos1 == std::string::npos) {
      return false;
   }
   const auto &binSettlementId = BinaryData::CreateFromHex(index.substr(0, pos1));
   return (getAddressBySettlementId(binSettlementId) != nullptr);
}

bs::Address SettlementWallet::createAddressWithIndex(const std::string &index, bool persistent, AddressEntryType )
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
      , BinaryData::CreateFromHex(sellAuthKey), "", persistent)->getPrefixedHash();
}

bool SettlementWallet::containsAddress(const bs::Address &addr)
{
   return !getAddressIndex(addr).empty();
}

BinaryData SettlementWallet::signPayoutTXRequest(const bs::core::wallet::TXSignRequest &req, const KeyPair &keys
   , const BinaryData &settlementId)
{
   const auto addr = getAddressBySettlementId(settlementId);
   if (!addr) {
      throw std::runtime_error("failed to find address for settlementId " + settlementId.toHexStr());
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

std::shared_ptr<AddressEntry> SettlementWallet::getAddressEntryForAddr(const BinaryData &addr)
{
   const auto &itAddrEntry = addrEntryByAddr_.find(addr);
   if (itAddrEntry == addrEntryByAddr_.end()) {
      return nullptr;
   }
   return itAddrEntry->second;
}

SecureBinaryData SettlementWallet::getPublicKeyFor(const bs::Address &addr)
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

KeyPair SettlementWallet::getKeyPairFor(const bs::Address &, const SecureBinaryData &)
{
   return {};
}
