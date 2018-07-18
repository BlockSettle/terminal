#ifndef __WALLETS_MANAGER_H__
#define __WALLETS_MANAGER_H__

#include <memory>
#include <vector>
#include <unordered_map>
#include <QString>
#include <QObject>
#include <QMutex>
#include <QDateTime>
#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "EncryptionUtils.h"
#include "SettlementWallet.h"


namespace spdlog {
   class logger;
}
namespace bs {
   namespace hd {
      class Wallet;
      class DummyWallet;
   }
}
class ApplicationSettings;


class WalletsManager : public QObject
{
   Q_OBJECT
public:
   using load_progress_delegate = std::function<void(int)>;
   using wallet_gen_type = std::shared_ptr<bs::Wallet>;     // Generic wallet interface
   using hd_wallet_type = std::shared_ptr<bs::hd::Wallet>;

public:
   WalletsManager(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<ArmoryConnection> &, bool preferWatchingOnly = true);
   WalletsManager(const std::shared_ptr<spdlog::logger> &);
   ~WalletsManager() noexcept;

   WalletsManager(const WalletsManager&) = delete;
   WalletsManager& operator = (const WalletsManager&) = delete;
   WalletsManager(WalletsManager&&) = delete;
   WalletsManager& operator = (WalletsManager&&) = delete;

   void Reset();

   void LoadWallets(NetworkType, const QString &walletsPath, const load_progress_delegate &progressDelegate = nullptr);
   void BackupWallet(const hd_wallet_type &, const std::string &targetDir) const;
   void AddWallet(const hd_wallet_type &, const QString &walletsPath);

   size_t GetWalletsCount() const { return wallets_.size(); }
   bool HasPrimaryWallet() const;
   bool HasSettlementWallet() const { return (settlementWallet_ != nullptr); }
   hd_wallet_type GetPrimaryWallet() const;
   std::shared_ptr<bs::hd::DummyWallet> GetDummyWallet() const { return hdDummyWallet_; }
   wallet_gen_type GetWallet(const unsigned int index) const;
   wallet_gen_type GetWalletById(const std::string& walletId) const;
   wallet_gen_type GetWalletByAddress(const bs::Address &addr) const;
   wallet_gen_type GetDefaultWallet() const;
   wallet_gen_type GetCCWallet(const std::string &cc);

   bool CreateSettlementWallet(NetworkType, const QString &walletsPath);

   size_t GetHDWalletsCount() const { return hdWalletsId_.size(); }
   const hd_wallet_type GetHDWallet(const unsigned int index) const;
   const hd_wallet_type GetHDWalletById(const std::string &walletId) const;
   const hd_wallet_type GetHDRootForLeaf(const std::string &walletId) const;
   bool WalletNameExists(const std::string &walletName) const;

   bool DeleteWalletFile(const wallet_gen_type& wallet);
   bool DeleteWalletFile(const hd_wallet_type& wallet);

   const std::shared_ptr<bs::SettlementWallet> GetSettlementWallet() const { return settlementWallet_; }

   void SetUserId(const BinaryData &userId);
   const wallet_gen_type GetAuthWallet() const { return authAddressWallet_; }

   bool IsArmoryReady() const;
   bool IsReadyForTrading() const;

   BTCNumericTypes::balance_type GetSpendableBalance() const;
   BTCNumericTypes::balance_type GetUnconfirmedBalance() const;
   BTCNumericTypes::balance_type GetTotalBalance() const;

   uint32_t GetTopBlockHeight() const;

   void RegisterSavedWallets();

   bool IsTransactionVerified(const ClientClasses::LedgerEntry &);
   bool GetTransactionDirection(Tx, const std::shared_ptr<bs::Wallet> &
      , std::function<void(bs::Transaction::Direction)>);
   bool GetTransactionMainAddress(const Tx &, const std::shared_ptr<bs::Wallet> &
      , bool isReceiving, std::function<void(QString)>);

   hd_wallet_type CreateWallet(const std::string& name, const std::string& description
      , bs::wallet::Seed, const QString &walletsPath
      , const SecureBinaryData &password = {}, bool primary = false);
   void AdoptNewWallet(const hd_wallet_type &, const QString &walletsPath);

   bool estimatedFeePerByte(const unsigned int blocksToWait, std::function<void(float)>) const;

   bool getNewTransactions() const;

   std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>> GetAddressesInAllWallets() const;

signals:
   void walletChanged();
   void walletsReady();
   void walletsLoaded();
   void walletReady(const QString &walletId);
   void newWalletAdded(const std::string &walletId);
   void authWalletChanged();
   void blockchainEvent();
   void info(const QString &title, const QString &text) const;
   void error(const QString &title, const QString &text) const;
   void walletImportStarted(const std::string &walletId);
   void walletImportFinished(const std::string &walletId);
   void newTransactions(std::vector<ClientClasses::LedgerEntry>) const;

public slots:
   void onCCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr);
   void onCCInfoLoaded();
   void onWalletImported(const std::string &walletId) { emit walletImportFinished(walletId); }

private slots:
   void onZeroConfReceived(ArmoryConnection::ReqIdType);
   void onBroadcastZCError(const QString &txHash, const QString &errMsg);
   void onWalletReady(const QString &walletId);
   void onHDLeafAdded(QString id);
   void onHDLeafDeleted(QString id);
   void onRefresh();
   void onStateChanged(ArmoryConnection::State);

private:
   bool empty() const { return (wallets_.empty() && !settlementWallet_); }

   // notification from block data manager listener ( WalletsManagerBlockListener )
   void ResumeRescan();
   void UpdateSavedWallets();

   bool IsWalletFile(const QString& fileName) const;
   void SaveWallet(const wallet_gen_type& newWallet);
   void SaveWallet(const hd_wallet_type& wallet);
   void AddWallet(const wallet_gen_type& wallet, bool isHDLeaf = false);
   void EraseWallet(const wallet_gen_type &wallet);
   void SetAuthWalletFrom(const hd_wallet_type &);

   void updateTxDirCache(const BinaryData &txHash, bs::Transaction::Direction
      , std::function<void(bs::Transaction::Direction)>);
   void updateTxDescCache(const BinaryData &txHash, const QString &, std::function<void(QString)>);

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<spdlog::logger>        logger_;
   const bool                             preferWatchingOnly_;

   using wallet_container_type = std::unordered_map<std::string, wallet_gen_type>;
   using hd_wallet_container_type = std::unordered_map<std::string, hd_wallet_type>;

   unsigned int                           nbBackupFilesToKeep_ = 10;
   hd_wallet_container_type               hdWallets_;
   std::shared_ptr<bs::hd::DummyWallet>   hdDummyWallet_;
   std::unordered_set<std::string>        walletNames_;
   wallet_container_type                  wallets_;
   mutable QMutex                         mtxWallets_;
   std::set<QString>                      readyWallets_;
   std::vector<BinaryData>                walletsId_;
   std::vector<std::string>               hdWalletsId_;
   wallet_gen_type                        authAddressWallet_;
   BinaryData                             userId_;
   std::shared_ptr<bs::SettlementWallet>  settlementWallet_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::unordered_map<std::string, std::string> ccSecurities_;

   std::map<BinaryData, bs::Transaction::Direction>   txDirections_;
   mutable std::atomic_flag      txDirLock_ = ATOMIC_FLAG_INIT;
   std::map<BinaryData, QString> txDesc_;
   mutable std::atomic_flag      txDescLock_ = ATOMIC_FLAG_INIT;

   mutable std::map<unsigned int, float>     feePerByte_;
   mutable std::map<unsigned int, QDateTime> lastFeePerByte_;
};

#endif // __WALLETS_MANAGER_H__
