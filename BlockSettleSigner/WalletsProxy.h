#ifndef __WALLETS_PROXY_H__
#define __WALLETS_PROXY_H__

#include <memory>
#include <QObject>
#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class SignerSettings;


class WalletsProxy : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool primaryWalletExists READ primaryWalletExists NOTIFY walletsChanged)
   Q_PROPERTY(bool loaded READ walletsLoaded NOTIFY walletsChanged)
   Q_PROPERTY(QStringList walletNames READ walletNames NOTIFY walletsChanged)
   Q_PROPERTY(QString defaultBackupLocation READ defaultBackupLocation NOTIFY walletsChanged)

public:
   WalletsProxy(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<bs::core::WalletsManager> &
      , const std::shared_ptr<SignerSettings> &);

   Q_INVOKABLE bool createWallet(bool isPrimary, bs::wallet::QSeed *
                                 , bs::hd::WalletInfo *
                                 , bs::wallet::QPasswordData *);

   Q_INVOKABLE bool deleteWallet(const QString &walletId);

   Q_INVOKABLE bool changePassword(const QString &walletId
                                   , bs::wallet::QPasswordData *oldPasswordData
                                   , bs::wallet::QPasswordData *newPasswordData);

   Q_INVOKABLE bool addEidDevice(const QString &walletId
                                   , bs::wallet::QPasswordData *oldPasswordData
                                   , bs::wallet::QPasswordData *newPasswordData);

   Q_INVOKABLE bool removeEidDevice(const QString &walletId
                                   , bs::wallet::QPasswordData *oldPasswordData
                                   , int removedIndex);

   Q_INVOKABLE QString getWoWalletFile(const QString &walletId) const;

   Q_INVOKABLE bool exportWatchingOnly(const QString &walletId
                                       , QString path
                                       , bs::wallet::QPasswordData *passwordData) const;

   Q_INVOKABLE bool backupPrivateKey(const QString &walletId
                                     , QString fileName
                                     , bool isPrintable
                                     , bs::wallet::QPasswordData *passwordData) const;


   Q_INVOKABLE QString getRootWalletId(const QString &walletId) const;
   Q_INVOKABLE QString getRootWalletName(const QString &walletId) const;

   Q_INVOKABLE int indexOfWalletId(const QString &walletId) const;
   Q_INVOKABLE QString walletIdForIndex(int) const;

   Q_INVOKABLE bool walletNameExists(const QString& name) const;

   bool walletsLoaded() const { return walletsLoaded_; }

   QString defaultBackupLocation() const;

signals:
   void walletError(const QString &walletId, const QString &errMsg) const;
   void walletsChanged();

private slots:
   void onWalletsChanged();

private:
   bool primaryWalletExists() const;
   std::shared_ptr<bs::core::hd::Wallet> getRootForId(const QString &walletId) const;
   QStringList walletNames() const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   std::shared_ptr<SignerSettings>  settings_;
   bool walletsLoaded_ = false;
};

#endif // __WALLETS_PROXY_H__
