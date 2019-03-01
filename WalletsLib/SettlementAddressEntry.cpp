#include "SettlementAddressEntry.h"

using namespace bs::core;

std::shared_ptr<SettlementAddressEntry> SettlementAssetEntry::getAddressEntry(const std::shared_ptr<SettlementAssetEntry> &assetPtr)
{
   std::shared_ptr<SettlementAssetEntry> sae = std::dynamic_pointer_cast<SettlementAssetEntry>(assetPtr);
   if (sae == nullptr) {
      throw AssetException("Asset entry is not SettlementAssetEntry");
   }
   std::shared_ptr<SettlementAddressEntry> addr;
   {
      switch (assetPtr->addressType()) {
      case AddressEntryType_Default:
      case AddressEntryType_Multisig:
         addr = std::make_shared<SettlementAddressEntry>(sae);
         break;

      case AddressEntryType_P2SH:
         addr = std::make_shared<SettlementAddressEntry_P2SH>(sae);
         break;

      case AddressEntryType_P2WSH:
         addr = std::make_shared<SettlementAddressEntry_P2WSH>(sae);
         break;

      default:
         throw AssetException("Unsupported address entry type");
      }
   }
   return addr;
}

std::pair<bs::Address, std::shared_ptr<bs::core::GenericAsset>> SettlementAssetEntry::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   const auto assetType = static_cast<AssetEntryType>(brrVal.get_uint8_t());
   if (assetType == AssetEntryType_Single) {
      return bs::core::PlainAsset::deserialize(value);
   }
   const auto id = brrVal.get_int32_t();

   uint64_t len = brrVal.get_var_int();
   const auto settlementId = brrVal.get_BinaryData(len);

   len = brrVal.get_var_int();
   const auto buyAuthPubKey = brrVal.get_BinaryData(len);

   len = brrVal.get_var_int();
   const auto sellAuthPubKey = brrVal.get_BinaryData(len);

   if (settlementId.isNull() || buyAuthPubKey.isNull() || sellAuthPubKey.isNull()) {
      throw AssetException("SettlementAssetEntry: invalid data in DB");
   }
   auto asset = std::make_shared<SettlementAssetEntry>(settlementId, buyAuthPubKey, sellAuthPubKey, id);

   if (brrVal.getSizeRemaining() > 0) {
      len = brrVal.get_var_int();
      const auto script = brrVal.get_BinaryData(len);
      if (!script.isNull()) {
         asset->setScript(script);
         asset->doNotCommit();
      }
   }

   if (brrVal.getSizeRemaining() > 0) {
      const auto addrType = brrVal.get_uint32_t();
      asset->addrType_ = static_cast<AddressEntryType>(addrType);
   }

   return { getAddressEntry(asset)->getPrefixedHash(), asset };
}

BinaryData SettlementAssetEntry::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(static_cast<uint8_t>(getType()));
   bw.put_int32_t(id_);

   bw.put_var_int(settlementId_.getSize());
   bw.put_BinaryData(settlementId_);

   bw.put_var_int(buyAuthPubKey_.getSize());
   bw.put_BinaryData(buyAuthPubKey_);

   bw.put_var_int(sellAuthPubKey_.getSize());
   bw.put_BinaryData(sellAuthPubKey_);

   bw.put_var_int(script_.getSize());
   bw.put_BinaryData(script_);

   uint32_t addrType = static_cast<uint32_t>(addrType_);
   bw.put_uint32_t(addrType);

   return bw.getData();
}

const BinaryData &SettlementAssetEntry::script() const
{
   if (script_.isNull()) {
      const BinaryData &buyChainKey = buyChainedPubKey();
      const BinaryData &sellChainKey = sellChainedPubKey();

      BinaryWriter script;
      script.put_uint8_t(OP_1);
      script.put_uint8_t((uint8_t)buyChainKey.getSize());
      script.put_BinaryData(buyChainKey);
      script.put_uint8_t((uint8_t)sellChainKey.getSize());
      script.put_BinaryData(sellChainKey);
      script.put_uint8_t(OP_2);
      script.put_uint8_t(OP_CHECKMULTISIG);

      script_ = script.getData();
   }
   return script_;
}

const BinaryData &SettlementAssetEntry::buyChainedPubKey() const
{
   if (buyChainedPubKey_.isNull()) {
      CryptoECDSA crypto;
      buyChainedPubKey_ = crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(buyAuthPubKey()), settlementId()));
   }
   return buyChainedPubKey_;
}

const BinaryData &SettlementAssetEntry::sellChainedPubKey() const
{
   if (sellChainedPubKey_.isNull()) {
      CryptoECDSA crypto;
      sellChainedPubKey_ = crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(sellAuthPubKey()), settlementId()));
   }
   return sellChainedPubKey_;
}

const BinaryData &SettlementAssetEntry::prefixedHash() const
{
   if (hash_.isNull()) {
      hash_.append(NetworkConfig::getScriptHashPrefix());
      hash_.append(hash());
   }
   return hash_;
}

const BinaryData &SettlementAssetEntry::p2wshScript() const
{
   if (p2wshScript_.isNull()) {
      const auto hash256 = BtcUtils::getSha256(script());
      Recipient_P2WSH recipient(hash256, 0);
      const auto &script = recipient.getSerializedScript();
      p2wshScript_ = script.getSliceCopy(9, (uint32_t)script.getSize() - 9);
   }
   return p2wshScript_;
}

const BinaryData &SettlementAssetEntry::p2wsHash() const
{
   if (p2wsHash_.isNull()) {
      p2wsHash_ = BtcUtils::getHash160(p2wshScript());
   }
   return p2wsHash_;
}

const BinaryData &SettlementAssetEntry::prefixedP2SHash() const
{
   if (prefixedP2SHash_.isNull()) {
      prefixedP2SHash_.append(NetworkConfig::getScriptHashPrefix());
      prefixedP2SHash_.append(p2wsHash());
   }
   return prefixedP2SHash_;
}

const std::vector<BinaryData> &SettlementAssetEntry::supportedAddresses() const
{
   if (supportedAddresses_.empty()) {
      supportedAddresses_ = { script(), p2wshScript() };
   }
   return supportedAddresses_;
}

const std::vector<BinaryData> &SettlementAssetEntry::supportedAddrHashes() const
{
   if (supportedHashes_.empty()) {
      BinaryData p2wshPrefixed;
      p2wshPrefixed.append(uint8_t(SCRIPT_PREFIX_P2WSH));
      p2wshPrefixed.append(BtcUtils::getSha256(script()));
      supportedHashes_ = { prefixedHash(), prefixedP2SHash(), p2wshPrefixed };
   }
   return supportedHashes_;
}


const BinaryData &SettlementAddressEntry_P2WSH::getHash() const
{
   if (hash_.isNull()) {
      hash_ = BtcUtils::getSha256(ae_->script());
   }
   return hash_;
}

const BinaryData &SettlementAddressEntry_P2WSH::getPrefixedHash(void) const
{
   if (prefixedHash_.isNull()) {
      prefixedHash_.append(uint8_t(SCRIPT_PREFIX_P2WSH));
      prefixedHash_.append(getHash());
   }
   return prefixedHash_;
}
