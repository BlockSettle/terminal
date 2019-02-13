#ifndef __BS_SETTLEMENT_WALLET_H__
#define __BS_SETTLEMENT_WALLET_H__

#include <atomic>
#include <string>
#include <vector>
#include <unordered_set>

#include <QObject>

#include "Addresses.h"
#include "Assets.h"
#include "BlockDataManagerConfig.h"
#include "BtcDefinitions.h"
#include "PlainWallet.h"
#include "SettlementAddressEntry.h"

namespace spdlog {
   class logger;
};

namespace bs {
   class SettlementAddressEntry;
   class SettlementMonitorQtSignals;
   class SettlementMonitorCb;

   class SettlementWallet : public PlainWallet
   {
      Q_OBJECT

   public:
      SettlementWallet(const std::shared_ptr<spdlog::logger> &logger = nullptr);
      SettlementWallet(const std::string &filename, const std::shared_ptr<spdlog::logger> &logger = nullptr);
      ~SettlementWallet() override = default;

      SettlementWallet(const SettlementWallet&) = delete;
      SettlementWallet(SettlementWallet&&) = delete;
      SettlementWallet& operator = (const SettlementWallet&) = delete;
      SettlementWallet& operator = (SettlementWallet&&) = delete;

      std::shared_ptr<bs::SettlementAddressEntry> getExistingAddress(const BinaryData &settlementId);

      std::shared_ptr<SettlementAddressEntry> newAddress(const BinaryData &settlementId
         , const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey, const std::string &comment = {});
      bool containsAddress(const bs::Address &addr) override;

      wallet::Type GetType() const override { return wallet::Type::Settlement; }

      static std::string fileNamePrefix() { return "settlement_"; }
      std::string getFileName(const std::string &dir) const override;

      bool GetInputFor(const std::shared_ptr<SettlementAddressEntry> &, std::function<void(UTXO)>, bool allowZC = true);
      uint64_t GetEstimatedFeeFor(UTXO input, const bs::Address &recvAddr, float feePerByte);

      bs::wallet::TXSignRequest CreatePayoutTXRequest(const UTXO &, const bs::Address &recvAddr, float feePerByte);
      UTXO GetInputFromTX(const std::shared_ptr<SettlementAddressEntry> &, const BinaryData &payinHash, const double amount) const;
      BinaryData SignPayoutTXRequest(const bs::wallet::TXSignRequest &, const KeyPair &, const BinaryData &settlementId
         , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey);

      std::shared_ptr<ResolverFeed> GetResolver(const SecureBinaryData &) override { return nullptr; }   // can't resolve without external data
      std::shared_ptr<ResolverFeed> GetPublicKeyResolver() override { return nullptr; }   // no public keys are stored

      bs::Address GetNewExtAddress(AddressEntryType) override { return {}; }  // can't generate address without input data
      bs::Address GetNewIntAddress(AddressEntryType) override { return {}; }  // can't generate address without input data

      std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
      std::string GetAddressIndex(const bs::Address &) override;
      bool AddressIndexExists(const std::string &index) const override;
      bs::Address CreateAddressWithIndex(const std::string &index, AddressEntryType, bool signal = true) override;

      SecureBinaryData GetPublicKeyFor(const bs::Address &) override;
      KeyPair GetKeyPairFor(const bs::Address &, const SecureBinaryData &password) override;

      void RefreshWallets(const std::vector<BinaryData>& ids);

      // return monitor that send QT signals and subscribed to zc/new block notification via qt
      bool createMonitorQtSignals(const std::shared_ptr<SettlementAddressEntry> &addr
         , const std::shared_ptr<spdlog::logger>& logger
         , const std::function<void (const std::shared_ptr<SettlementMonitorQtSignals>&)>& userCB);

      // pure callback monitor. you should manually ask to update and set
      // callbacks to get notifications
      bool createMonitorCb(const std::shared_ptr<SettlementAddressEntry> &addr
         , const std::shared_ptr<spdlog::logger>& logger
         , const std::function<void (const std::shared_ptr<SettlementMonitorCb>&)>& userCB);

   private:
      using CreateMonitorCallback = std::function<void(const std::shared_ptr<AsyncClient::BtcWallet>&)>;
      bool createMonitorCommon(const std::shared_ptr<SettlementAddressEntry> &addr
         , const std::shared_ptr<spdlog::logger>& logger
         , const CreateMonitorCallback& internalCB);

   protected:
      int addAddress(const bs::Address &, std::shared_ptr<GenericAsset> asset) override;
      int addAddress(const std::shared_ptr<SettlementAddressEntry> &, const std::shared_ptr<SettlementAssetEntry> &);
      std::pair<bs::Address, std::shared_ptr<GenericAsset>> deserializeAsset(BinaryDataRef ref) override {
         return SettlementAssetEntry::deserialize(ref);
      }

      AddressEntryType getAddrTypeForAddr(const BinaryData &addr) override;

   private:
      std::shared_ptr<bs::SettlementAddressEntry> getAddressBySettlementId(const BinaryData &settlementId) const;
      bool createTempWalletForAsset(const std::shared_ptr<SettlementAssetEntry>& asset);

      std::shared_ptr<AsyncClient::BtcWallet> GetSettlementAddressWallet(const int addressIndex) const;

      void CompleteMonitorCreations(int addressIndex, const std::shared_ptr<AsyncClient::BtcWallet>&);

   private:
      mutable std::atomic_flag                           lockAddressMap_ = ATOMIC_FLAG_INIT;
      std::map<bs::Address, std::shared_ptr<SettlementAddressEntry>>    addrEntryByAddr_;
      std::map<BinaryData, std::shared_ptr<bs::SettlementAddressEntry>> addressBySettlementId_;

      // all 3 collections guarded by same lock
      mutable std::atomic_flag                                 lockWalletsMap_ = ATOMIC_FLAG_INIT;
      // wallet per address
      std::map<int, std::shared_ptr<AsyncClient::BtcWallet>>   settlementAddressWallets_;
      // wallet that are now in phase of registration on armory side
      std::unordered_map<std::string, std::function<void()>>   pendingWalletRegistrations_;
      // pending requests to create monitor on wallet that is not completely registered on armory
      std::unordered_map<int, CreateMonitorCallback>           pendingMonitorCreations_;
   };
}  //namespace bs

#endif //__BS_SETTLEMENT_WALLET_H__
