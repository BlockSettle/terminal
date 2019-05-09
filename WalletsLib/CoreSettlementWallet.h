#ifndef BS_CORE_SETTLEMENT_WALLET_H
#define BS_CORE_SETTLEMENT_WALLET_H

#include <atomic>
#include <string>
#include <vector>
#include <unordered_set>

#include "Addresses.h"
#include "Assets.h"
#include "BlockDataManagerConfig.h"
#include "BtcDefinitions.h"
#include "CorePlainWallet.h"
#include "SettlementAddressEntry.h"

namespace spdlog {
   class logger;
}

namespace bs {
   class SettlementAddressEntry;

   namespace core {

      class SettlementWallet : public PlainWallet
      {
      public:
         SettlementWallet(NetworkType, const std::shared_ptr<spdlog::logger> &logger = nullptr);
         SettlementWallet(NetworkType, const std::string &filename
            , const std::shared_ptr<spdlog::logger> &logger = nullptr);
         ~SettlementWallet() override = default;

         SettlementWallet(const SettlementWallet&) = delete;
         SettlementWallet(SettlementWallet&&) = delete;
         SettlementWallet& operator = (const SettlementWallet&) = delete;
         SettlementWallet& operator = (SettlementWallet&&) = delete;

         std::shared_ptr<SettlementAddressEntry> getExistingAddress(const BinaryData &settlementId);

         std::shared_ptr<SettlementAddressEntry> newAddress(const BinaryData &settlementId
            , const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey
            , const std::string &comment = {}, bool persistent = true);
         bool containsAddress(const bs::Address &addr) override;

         wallet::Type type() const override { return wallet::Type::Settlement; }

         static std::string fileNamePrefix() { return "settlement_"; }
         std::string getFileName(const std::string &dir) const override;

         BinaryData signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const KeyPair &
            , const BinaryData &settlementId);

         bs::Address getNewExtAddress(AddressEntryType) override { return {}; }  // can't generate address without input data
         bs::Address getNewIntAddress(AddressEntryType) override { return {}; }  // can't generate address without input data

         std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
         std::string getAddressIndex(const bs::Address &) override;
         bool addressIndexExists(const std::string &index) const override;
         bs::Address createAddressWithIndex(const std::string &index, bool persistent, AddressEntryType);

         SecureBinaryData getPublicKeyFor(const bs::Address &) override;
         KeyPair getKeyPairFor(const bs::Address &, const SecureBinaryData &password);

      protected:
         int addAddress(const bs::Address &, const std::shared_ptr<GenericAsset> &asset = nullptr) override;
         int addAddress(const std::shared_ptr<SettlementAddressEntry> &, const std::shared_ptr<SettlementAssetEntry> &);
         std::pair<bs::Address, std::shared_ptr<GenericAsset>> deserializeAsset(BinaryDataRef ref) override {
            return SettlementAssetEntry::deserialize(ref);
         }

//         AddressEntryType getAddrTypeForAddr(const BinaryData &addr) override;

      private:
         std::shared_ptr<SettlementAddressEntry> getAddressBySettlementId(const BinaryData &settlementId) const;

      private:
         mutable std::atomic_flag                           lockAddressMap_ = ATOMIC_FLAG_INIT;
         std::map<bs::Address, std::shared_ptr<SettlementAddressEntry>> addrEntryByAddr_;
         std::map<BinaryData, std::shared_ptr<SettlementAddressEntry>>  addressBySettlementId_;
      };

   }  //namespace core
}  //namespace bs

#endif //BS_CORE_SETTLEMENT_WALLET_H
