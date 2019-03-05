#ifndef __SETTLEMENT_ADDRESS_ENTRY_H__
#define __SETTLEMENT_ADDRESS_ENTRY_H__

#include "BinaryData.h"
#include <memory>
#include "CorePlainWallet.h"

namespace bs {
   namespace core {
      class SettlementAddressEntry;

      ////////////////////////////////////////////////////////////////////////////////
      class SettlementAssetEntry : public bs::core::GenericAsset
      {
      public:
         SettlementAssetEntry(const BinaryData &settlementId, const BinaryData &buyAuthPubKey
            , const BinaryData &sellAuthPubKey, int id = -1)
            : GenericAsset(AssetEntryType_Multisig, id)
            , settlementId_(settlementId), buyAuthPubKey_(buyAuthPubKey), sellAuthPubKey_(sellAuthPubKey)
            , addrType_(AddressEntryType_P2WSH)
         {}
         ~SettlementAssetEntry() override = default;

         static std::shared_ptr<SettlementAddressEntry> getAddressEntry(const std::shared_ptr<SettlementAssetEntry> &);
         static std::pair<bs::Address, std::shared_ptr<GenericAsset>> deserialize(BinaryDataRef value);
         BinaryData serialize(void) const override;

         bool hasPrivateKey(void) const override { return false; }
         const BinaryData &getPrivateEncryptionKeyId(void) const override { return {}; }

         const BinaryData &settlementId() const { return settlementId_; }
         const BinaryData &buyAuthPubKey() const { return buyAuthPubKey_; }
         const BinaryData &buyChainedPubKey() const;
         const BinaryData &sellAuthPubKey() const { return sellAuthPubKey_; }
         const BinaryData &sellChainedPubKey() const;

         AddressEntryType addressType() const { return addrType_; }

         const BinaryData &script() const;
         void setScript(const BinaryData &script) { script_ = script; }
         const BinaryData &p2wshScript() const;

         BinaryData hash() const { return BtcUtils::hash160(script()); }
         const BinaryData &prefixedHash() const;
         const BinaryData &p2wsHash() const;
         const BinaryData &prefixedP2SHash() const;

         const std::vector<BinaryData> &supportedAddresses() const;
         const std::vector<BinaryData> &supportedAddrHashes() const;

      private:
         BinaryData  settlementId_;
         BinaryData  buyAuthPubKey_;
         BinaryData  sellAuthPubKey_;
         AddressEntryType     addrType_;
         mutable BinaryData   script_;
         mutable BinaryData   p2wshScript_;
         mutable BinaryData   p2wshScriptH160_;
         mutable BinaryData   hash_;
         mutable BinaryData   p2wsHash_;
         mutable BinaryData   prefixedP2SHash_;
         mutable BinaryData   buyChainedPubKey_;
         mutable BinaryData   sellChainedPubKey_;
         mutable std::vector<BinaryData>  supportedHashes_, supportedAddresses_;
      };

      ////////////////////////////////////////////////////////////////////////////////
      class SettlementAddressEntry : public AddressEntry
      {
      public:
         SettlementAddressEntry(const std::shared_ptr<SettlementAssetEntry> &ae, AddressEntryType aeType = AddressEntryType_Multisig)
            : AddressEntry(aeType), ae_(ae) {}
         ~SettlementAddressEntry() noexcept override = default;

         const std::shared_ptr<SettlementAssetEntry>  getAsset() const { return ae_; }
         const BinaryData &getAddress() const override { return ae_->script(); }
         const BinaryData &getScript() const override { return ae_->script(); }
         const BinaryData &getPreimage(void) const override { return ae_->script(); }
         const BinaryData &getPrefixedHash() const override { return ae_->prefixedHash(); }
         const BinaryData &getHash() const override { return ae_->hash(); }
         std::shared_ptr<ScriptRecipient> getRecipient(uint64_t val) const override { return bs::Address(getPrefixedHash()).getRecipient(val); }
         size_t getInputSize(void) const override { return getAddress().getSize() + 2 + 73 * 1/*m*/ + 40; }

         const BinaryData& getID(void) const override { return ae_->getID(); }
         int getIndex() const { return ae_->id(); }

      protected:
         std::shared_ptr<SettlementAssetEntry>  ae_;
      };

      ////////////////////////////////////////////////////////////////////////////////
      class SettlementAddressEntry_P2SH : public SettlementAddressEntry
      {
      public:
         SettlementAddressEntry_P2SH(const std::shared_ptr<SettlementAssetEntry> &ae)
            : SettlementAddressEntry(ae, AddressEntryType_P2SH) {}

         const BinaryData &getPrefixedHash(void) const override { return ae_->prefixedP2SHash(); }
         const BinaryData &getHash() const override { return ae_->p2wsHash(); }
         std::shared_ptr<ScriptRecipient> getRecipient(uint64_t val) const override { return std::make_shared<Recipient_P2SH>(ae_->p2wsHash(), val); }
         size_t getInputSize(void) const override { return 75; }
      };

      ////////////////////////////////////////////////////////////////////////////////
      class SettlementAddressEntry_P2WSH : public SettlementAddressEntry
      {
      public:
         SettlementAddressEntry_P2WSH(const std::shared_ptr<SettlementAssetEntry> &ae)
            : SettlementAddressEntry(ae, AddressEntryType_P2WSH) {}

         const BinaryData &getPrefixedHash(void) const override;
         const BinaryData &getHash() const override;
         const BinaryData &getAddress() const override { return BtcUtils::scrAddrToSegWitAddress(getHash()); }
         std::shared_ptr<ScriptRecipient> getRecipient(uint64_t val) const override { return std::make_shared<Recipient_P2WSH>(getHash(), val); }
         size_t getInputSize(void) const override { return 41; }

      private:
         mutable BinaryData   hash_;
         mutable BinaryData   prefixedHash_;
      };

   }  //namespace core
} // namespace bs
#endif // __SETTLEMENT_ADDRESS_ENTRY_H__
