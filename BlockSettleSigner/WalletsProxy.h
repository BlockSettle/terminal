#ifndef __WALLETS_PROXY_H__
#define __WALLETS_PROXY_H__

#include <memory>
#include <QObject>
#include "MetaData.h"


namespace spdlog {
   class logger;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
   class Wallet;
}
class WalletsManager;


class WalletsProxy : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool primaryWalletExists READ primaryWalletExists NOTIFY walletsChanged)
   Q_PROPERTY(bool loaded READ walletsLoaded NOTIFY walletsChanged)
   Q_PROPERTY(QStringList walletNames READ walletNames NOTIFY walletsChanged)

public:
   WalletsProxy(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<WalletsManager> &);

   Q_INVOKABLE bool changePassword(const QString &walletId, const QString &oldPass, const QString &newPass);
   Q_INVOKABLE QString getWoWalletFile(const QString &walletId) const;
   Q_INVOKABLE bool exportWatchingOnly(const QString &walletId, QString path, const QString &password) const;
   Q_INVOKABLE bool backupPrivateKey(const QString &walletId, QString fileName, bool isPrintable
      , const QString &password) const;
   Q_INVOKABLE bool createWallet(const QString &name, const QString &desc, bool isPrimary, const QString &password);
   Q_INVOKABLE bool importWallet(const QString &name, const QString &desc, bool isPrimary, const QString &key
      , bool digitalBackup, const QString &password);
   Q_INVOKABLE bool deleteWallet(const QString &walletId);

   Q_INVOKABLE QString getRootWalletId(const QString &walletId) const;
   Q_INVOKABLE QString getRootWalletName(const QString &walletId) const;

   Q_INVOKABLE int indexOfWalletId(const QString &walletId) const;
   Q_INVOKABLE QString walletIdForIndex(int) const;

signals:
   void walletError(const QString &walletId, const QString &errMsg) const;
   void walletsChanged();

private slots:
   void onWalletsChanged();

private:
   bool primaryWalletExists() const;
   std::shared_ptr<bs::hd::Wallet> getRootForId(const QString &walletId) const;
   QStringList walletNames() const;
   bool walletsLoaded() const { return walletsLoaded_; }

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   bool walletsLoaded_ = false;
};

#endif // __WALLETS_PROXY_H__
