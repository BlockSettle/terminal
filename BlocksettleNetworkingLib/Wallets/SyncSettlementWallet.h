#ifndef BS_SYNC_SETTLEMENT_WALLET_H__
#define BS_SYNC_SETTLEMENT_WALLET_H__

#include <functional>
#include <string>
#include <vector>
#include <QObject>
#include "SettlementMonitor.h"
#include "SyncPlainWallet.h"

namespace spdlog {
   class logger;
};

namespace bs {
   class SettlementMonitorQtSignals;
   class SettlementMonitorCb;

   namespace sync {

      class SettlementWallet : public PlainWallet
      {
      public:
         SettlementWallet(const std::string &walletId, const std::string &name, const std::string &desc
            , WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &logger);
         ~SettlementWallet() override = default;

         SettlementWallet(const SettlementWallet&) = delete;
         SettlementWallet(SettlementWallet&&) = delete;
         SettlementWallet& operator = (const SettlementWallet&) = delete;
         SettlementWallet& operator = (SettlementWallet&&) = delete;

         bs::Address getExistingAddress(const BinaryData &settlementId);

         void newAddress(const CbAddress &, const BinaryData &settlementId
            , const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey, const std::string &comment = {});
         bool containsAddress(const bs::Address &addr) override;

         bs::core::wallet::Type type() const override { return bs::core::wallet::Type::Settlement; }

         void getNewExtAddress(const CbAddress &, AddressEntryType) override {}  // can't generate address without input data
         void getNewIntAddress(const CbAddress &, AddressEntryType) override {}  // can't generate address without input data

         std::string getAddressIndex(const bs::Address &) override;
         bool addressIndexExists(const std::string &index) const override;
         std::shared_ptr<SettlementAddress> getAddressEntryForAddr(const bs::Address &addr) const;

         void refreshWallets(const std::vector<BinaryData>& ids);

         // return monitor that send QT signals and subscribed to zc/new block notification via qt
         bool createMonitorQtSignals(const bs::Address &, const std::shared_ptr<spdlog::logger> &
            , const std::function<void(const std::shared_ptr<SettlementMonitorQtSignals>&)>& userCB);

         // pure callback monitor. you should manually ask to update and set
         // callbacks to get notifications
         bool createMonitorCb(const bs::Address &, const std::shared_ptr<spdlog::logger> &
            , const std::function<void(const std::shared_ptr<SettlementMonitorCb>&)>& userCB);

      private:
         using CreateMonitorCallback = std::function<void(const std::shared_ptr<AsyncClient::BtcWallet>&)>;
         bool createMonitorCommon(const bs::Address &, const std::shared_ptr<spdlog::logger> &
            , const CreateMonitorCallback& internalCB);

      private:
         bs::Address getAddressBySettlementId(const BinaryData &settlementId) const;

         bool createTempWalletForAddress(const bs::Address &);

         std::shared_ptr<AsyncClient::BtcWallet> getSettlementAddressWallet(const bs::Address &) const;

         void completeMonitorCreations(const bs::Address &, const std::shared_ptr<AsyncClient::BtcWallet> &);

      private:
         mutable std::atomic_flag                           lockAddressMap_ = ATOMIC_FLAG_INIT;
         std::map<bs::Address, std::shared_ptr<SettlementAddress>>    addrEntryByAddr_;
         std::map<BinaryData, bs::Address>   addrBySettlementId_;
         std::map<bs::Address, BinaryData>   settlementIdByAddr_;

         // all 3 collections guarded by same lock
         mutable std::atomic_flag                                 lockWalletsMap_ = ATOMIC_FLAG_INIT;
         // wallet per address
         std::map<bs::Address, std::shared_ptr<AsyncClient::BtcWallet>> settlementAddressWallets_;
         // wallet that are now in phase of registration on armory side
         std::map<std::string, std::function<void()>>   pendingWalletRegistrations_;
         // pending requests to create monitor on wallet that is not completely registered on armory
         std::map<bs::Address, CreateMonitorCallback>   pendingMonitorCreations_;
      };

   }  //namespace sync
}  //namespace bs

#endif //__BS_SETTLEMENT_WALLET_H__
