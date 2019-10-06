#ifndef BS_SYNC_WALLETS_MANAGER_H
#define BS_SYNC_WALLETS_MANAGER_H

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>

#include "ArmoryConnection.h"
#include "BSErrorCode.h"
#include "BTCNumericTypes.h"
#include "CoreWallet.h"
#include "SyncWallet.h"
#include "WalletSignerContainer.h"

namespace spdlog {
   class logger;
}

class ApplicationSettings;
class WalletSignerContainer;

namespace bs {
   namespace hd {
      class Wallet;
   }
   namespace sync {
      namespace hd {
         class Group;
         class Wallet;
         class DummyWallet;
      }
      class Wallet;

      class WalletsManager : public QObject, public ArmoryCallbackTarget, public WalletCallbackTarget
      {
         Q_OBJECT
      public:
         using CbProgress = std::function<void(int, int)>;
         using WalletPtr = std::shared_ptr<Wallet>;     // Generic wallet interface
         using HDWalletPtr = std::shared_ptr<hd::Wallet>;
         using GroupPtr = std::shared_ptr<hd::Group>;

         WalletsManager(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<ApplicationSettings>& appSettings
            , const std::shared_ptr<ArmoryConnection> &);
         ~WalletsManager() noexcept override;

         WalletsManager(const WalletsManager&) = delete;
         WalletsManager& operator = (const WalletsManager&) = delete;
         WalletsManager(WalletsManager&&) = delete;
         WalletsManager& operator = (WalletsManager&&) = delete;

         void setSignContainer(const std::shared_ptr<WalletSignerContainer> &container);
         void reset();

         void syncWallets(const CbProgress &cb = nullptr);

         bool isWalletsReady() const;

         size_t walletsCount() const { return wallets_.size(); }
         bool hasPrimaryWallet() const;
         HDWalletPtr getPrimaryWallet() const;
//         std::shared_ptr<hd::DummyWallet> getDummyWallet() const { return hdDummyWallet_; }
         std::vector<WalletPtr> getAllWallets() const;
         WalletPtr getWalletById(const std::string& walletId) const;
         WalletPtr getWalletByAddress(const bs::Address &addr) const;
         WalletPtr getDefaultWallet() const;
         GroupPtr getGroupByWalletId(const std::string &walletId) const;

         bool PromoteHDWallet(const std::string& walletId
            , const std::function<void(bs::error::ErrorCode result)> &cb = nullptr);
         bool CreateCCLeaf(const std::string &cc
            , const std::function<void(bs::error::ErrorCode result)> &cb = nullptr);
         bool createAuthLeaf(const std::function<void()> &);

         WalletPtr getCCWallet(const std::string &cc);

         const WalletPtr getAuthWallet() const { return authAddressWallet_; }

         void createSettlementLeaf(const bs::Address &authAddr
            , const std::function<void(const SecureBinaryData &)> &);

         size_t hdWalletsCount() const { return hdWalletsId_.size(); }
         const HDWalletPtr getHDWallet(unsigned) const;
         const HDWalletPtr getHDWalletById(const std::string &walletId) const;
         const HDWalletPtr getHDRootForLeaf(const std::string &walletId) const;
         bool walletNameExists(const std::string &walletName) const;
         bool isWatchingOnly(const std::string &walletId) const;

         // Do not use references here (could crash when underlying pointers are cleared)
         bool deleteWallet(WalletPtr);
         bool deleteWallet(HDWalletPtr);

         void setUserId(const BinaryData &userId);
         std::shared_ptr<CCDataResolver> ccResolver() const { return ccResolver_; }

         bool isArmoryReady() const;
         bool isReadyForTrading() const;

         BTCNumericTypes::balance_type getSpendableBalance() const;
         BTCNumericTypes::balance_type getUnconfirmedBalance() const;
         BTCNumericTypes::balance_type getTotalBalance() const;

         std::vector<std::string> registerWallets();
         void unregisterWallets();

         bool getTransactionDirection(Tx, const std::string &walletId
            , const std::function<void(Transaction::Direction, std::vector<bs::Address> inAddrs)> &);
         bool getTransactionMainAddress(const Tx &, const std::string &walletId
            , bool isReceiving, const std::function<void(QString, int)> &);

         void adoptNewWallet(const HDWalletPtr &);

         bool estimatedFeePerByte(const unsigned int blocksToWait, std::function<void(float)>, QObject *obj = nullptr);
         bool getFeeSchedule(const std::function<void(const std::map<unsigned int, float> &)> &);

         //run after registration to update address chain usage counters
         void trackAddressChainUse(std::function<void(bool)>);

         std::map<std::string, std::vector<bs::Address>> getAddressToWalletsMapping(const std::vector<UTXO> &) const;
         static std::shared_ptr<ResolverFeed> getPublicResolver(const std::map<bs::Address, BinaryData> &);

      signals:
         void CCLeafCreated(const std::string& ccName);
         void CCLeafCreateFailed(const std::string& ccName, bs::error::ErrorCode result);

         void AuthLeafCreated();
         void AuthLeafNotCreated();

         void walletPromotedToPrimary(const std::string& walletId);
         void walletPromotionFailed(const std::string& walletId, bs::error::ErrorCode result);

         void walletChanged(const std::string &walletId);
         void walletDeleted(const std::string &walletId);
         void walletAdded(const std::string &walletId);
         void walletsReady();
         void walletsSynchronized();
         void walletBalanceUpdated(const std::string &walletId);
         void walletMetaChanged(const std::string &walletId);
         void walletIsReady(const std::string &walletId);
         void newWalletAdded(const std::string &walletId);
         void authWalletChanged();
         void blockchainEvent();
         void info(const QString &title, const QString &text) const;
         void error(const QString &title, const QString &text) const;
         void walletImportStarted(const std::string &walletId);
         void walletImportFinished(const std::string &walletId);
         void newTransactions(std::vector<bs::TXEntry>) const;
         void invalidatedZCs(std::vector<bs::TXEntry>) const;

      public slots:
         void onCCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr);
         void onCCInfoLoaded();

      private:
         void onZCReceived(const std::vector<bs::TXEntry> &) override;
         void onZCInvalidated(const std::vector<bs::TXEntry> &) override;
         void onTxBroadcastError(const std::string &txHash, const std::string &errMsg) override;
         void onNewBlock(unsigned int height, unsigned int branchHeight) override;
         void onStateChanged(ArmoryState) override;

      private slots:
         void onHDWalletCreated(unsigned int id, std::shared_ptr<bs::sync::hd::Wallet>);
         void onWalletsListUpdated();
         void onAuthLeafAdded(const std::string &walletId);

      private:
         void addressAdded(const std::string &) override;
         void balanceUpdated(const std::string &) override;
         void walletReady(const std::string &) override;
         void walletCreated(const std::string &) override;
         void walletDestroyed(const std::string &) override;
         void walletReset(const std::string &) override;
         void scanComplete(const std::string &walletId) override;
         void metadataChanged(const std::string &walletId) override;

         bool empty() const { return wallets_.empty(); }

         void syncWallet(const bs::sync::WalletInfo &, const std::function<void()> &cbDone);
         void addWallet(const WalletPtr &, bool isHDLeaf = false);
         void addWallet(const HDWalletPtr &);
         void saveWallet(const WalletPtr &);
         void saveWallet(const HDWalletPtr &);
         void eraseWallet(const WalletPtr &);

         void updateTxDirCache(const std::string &txKey, Transaction::Direction
            , const std::vector<bs::Address> &inAddrs
            , std::function<void(Transaction::Direction, std::vector<bs::Address>)>);
         void updateTxDescCache(const std::string &txKey, const QString &, int
            , std::function<void(QString, int)>);

         void invokeFeeCallbacks(unsigned int blocks, float fee);

         BTCNumericTypes::balance_type getBalanceSum(
            const std::function<BTCNumericTypes::balance_type(const WalletPtr &)> &) const;

         void startWalletRescan(const HDWalletPtr &);

         using MaintQueueCb = std::function<void()>;
         void addToMaintQueue(const MaintQueueCb &);
         void maintenanceThreadFunc();

         void processCreatedCCLeaf(const std::string &cc, bs::error::ErrorCode result
            , const std::string &walletId);

         void processPromoteHDWallet(bs::error::ErrorCode result, const std::string& walletId);

      private:
         std::shared_ptr<WalletSignerContainer>         signContainer_;
         std::shared_ptr<spdlog::logger>        logger_;
         std::shared_ptr<ApplicationSettings>   appSettings_;
         std::shared_ptr<ArmoryConnection>      armoryPtr_;

         using wallet_container_type = std::unordered_map<std::string, WalletPtr>;
         using hd_wallet_container_type = std::unordered_map<std::string, HDWalletPtr>;

         hd_wallet_container_type            hdWallets_;
//         std::shared_ptr<hd::DummyWallet>    hdDummyWallet_;
         std::unordered_set<std::string>     walletNames_;
         wallet_container_type               wallets_;
         mutable QMutex                      mtxWallets_;
         std::set<std::string>               readyWallets_;
         bool     isReady_ = false;
         std::set<BinaryData>                walletsId_;
         std::set<std::string>               hdWalletsId_;
         WalletPtr                           authAddressWallet_;
         BinaryData                          userId_;
         std::set<std::string>               newWallets_;
         mutable std::unordered_map<std::string, GroupPtr>  groupsByWalletId_;

         class CCResolver : public CCDataResolver
         {
         public:
            std::string nameByWalletIndex(bs::hd::Path::Elem) const override;
            uint64_t lotSizeFor(const std::string &cc) const override;
            bs::Address genesisAddrFor(const std::string &cc) const override;
            std::string descriptionFor(const std::string &cc) const override;
            std::vector<std::string> securities() const override;

            void addData(const std::string &cc, uint64_t lotSize, const bs::Address &genAddr
               , const std::string &desc);

         private:
            struct CCInfo {
               std::string desc;
               uint64_t    lotSize;
               bs::Address genesisAddr;
            };
            std::unordered_map<std::string, CCInfo>   securities_;
            std::unordered_map<bs::hd::Path::Elem, std::string>   walletIdxMap_;
         };
         std::shared_ptr<CCResolver>   ccResolver_;

         std::unordered_map<std::string, std::pair<Transaction::Direction, std::vector<bs::Address>>> txDirections_;
         mutable std::atomic_flag      txDirLock_ = ATOMIC_FLAG_INIT;
         std::unordered_map<std::string, std::pair<QString, int>> txDesc_;
         mutable std::atomic_flag      txDescLock_ = ATOMIC_FLAG_INIT;

         mutable std::map<unsigned int, float>     feePerByte_;
         mutable std::map<unsigned int, QDateTime> lastFeePerByte_;
         std::map<QObject *, std::map<unsigned int
            , std::pair<QPointer<QObject>, std::function<void(float)>>>> feeCallbacks_;

         unsigned int createHdReqId_ = 0;

         std::atomic_bool  synchronized_{ false };

         std::atomic_bool           maintThreadRunning_{ false };
         std::deque<MaintQueueCb>   maintQueue_;
         std::thread                maintThread_;
         std::condition_variable    maintCV_;
         std::mutex                 maintMutex_;

      };

   }  //namespace sync
}  //namespace bs

#endif // BS_SYNC_WALLETS_MANAGER_H
