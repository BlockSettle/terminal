#ifndef __WALLETS_PROXY_H__
#define __WALLETS_PROXY_H__

#include <memory>
#include <QObject>
#include <QJSValue>
#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"
#include "SignerDefs.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class SignerAdapter;


class WalletsProxy : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool primaryWalletExists READ primaryWalletExists NOTIFY walletsChanged)
   Q_PROPERTY(bool loaded READ walletsLoaded NOTIFY walletsChanged)
   Q_PROPERTY(QStringList walletNames READ walletNames NOTIFY walletsChanged)
   Q_PROPERTY(QString defaultBackupLocation READ defaultBackupLocation NOTIFY walletsChanged)

public:
   WalletsProxy(const std::shared_ptr<spdlog::logger> &, SignerAdapter *);

   Q_INVOKABLE void createWallet(bool isPrimary, bs::wallet::QSeed *
      , bs::hd::WalletInfo *
      , bs::wallet::QPasswordData *, const QJSValue &jsCallback);

   Q_INVOKABLE void deleteWallet(const QString &walletId, const QJSValue &jsCallback);

   Q_INVOKABLE void changePassword(const QString &walletId
      , bs::wallet::QPasswordData *oldPasswordData
      , bs::wallet::QPasswordData *newPasswordData
      , const QJSValue &jsCallback);

   Q_INVOKABLE void addEidDevice(const QString &walletId
      , bs::wallet::QPasswordData *oldPasswordData
      , bs::wallet::QPasswordData *newPasswordData);

   Q_INVOKABLE void removeEidDevice(const QString &walletId
      , bs::wallet::QPasswordData *oldPasswordData
      , int removedIndex);

   Q_INVOKABLE QString getWoWalletFile(const QString &walletId) const;
   Q_INVOKABLE void importWoWallet(const QString &pathName, const QJSValue &jsCallback);

   Q_INVOKABLE void exportWatchingOnly(const QString &walletId
      , const QString &path, bs::wallet::QPasswordData *passwordData
      , const QJSValue &jsCallback);

   Q_INVOKABLE bool backupPrivateKey(const QString &walletId
      , QString fileName, bool isPrintable, bs::wallet::QPasswordData *passwordData
      , const QJSValue &jsCallback);

   Q_INVOKABLE QString getRootWalletId(const QString &walletId) const;
   Q_INVOKABLE QString getRootWalletName(const QString &walletId) const;

   Q_INVOKABLE int indexOfWalletId(const QString &walletId) const;
   Q_INVOKABLE QString walletIdForIndex(int) const;

   Q_INVOKABLE bool walletNameExists(const QString& name) const;
   Q_INVOKABLE bool isWatchingOnlyWallet(const QString& walletId) const;

   bool walletsLoaded() const { return walletsSynchronized_; }

   QString defaultBackupLocation() const;

signals:
   void walletError(const QString &walletId, const QString &errMsg) const;
   void walletsChanged();

private slots:
   void onWalletsChanged();
   void setWalletsManager();

private:
   bool primaryWalletExists() const;
   std::shared_ptr<bs::sync::hd::Wallet> getRootForId(const QString &walletId) const;
   QStringList walletNames() const;
   Q_INVOKABLE QJSValue invokeJsCallBack(QJSValue jsCallback, QJSValueList args);
   std::shared_ptr<bs::sync::hd::Wallet> getWoSyncWallet(const bs::sync::WatchingOnlyWallet &) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   SignerAdapter  *  adapter_{};
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   bool walletsSynchronized_ = false;
};

#endif // __WALLETS_PROXY_H__
