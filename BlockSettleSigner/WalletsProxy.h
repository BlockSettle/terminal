/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __WALLETS_PROXY_H__
#define __WALLETS_PROXY_H__

#include <memory>
#include <QObject>
#include <QJSValue>

#include "BSErrorCode.h"
#include "Wallets/SignerDefs.h"
#include "hwcommonstructure.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace hd {
      class WalletInfo;
   }
   namespace wallet {
      class QPasswordData;
      class QSeed;
   }
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class SignerAdapter;
class SignAdapterContainer;

class WalletsProxy : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool primaryWalletExists READ primaryWalletExists NOTIFY walletsChanged)
   Q_PROPERTY(bool loaded READ walletsLoaded NOTIFY walletsChanged)
   Q_PROPERTY(QStringList walletNames READ walletNames NOTIFY walletsChanged)
   Q_PROPERTY(QStringList priWalletNames READ priWalletNames NOTIFY walletsChanged)
   Q_PROPERTY(QString defaultBackupLocation READ defaultBackupLocation NOTIFY walletsChanged)
   Q_PROPERTY(bool hasCCInfo READ hasCCInfo NOTIFY ccInfoChanged)

public:
   WalletsProxy(const std::shared_ptr<spdlog::logger> &, SignerAdapter *);

   Q_INVOKABLE void createWallet(bool isPrimary, bool createLegacyLeaf, bs::wallet::QSeed *
      , bs::hd::WalletInfo *
      , bs::wallet::QPasswordData *, const QJSValue &jsCallback);

   Q_INVOKABLE void deleteWallet(const QString &walletId, const QJSValue &jsCallback);

   Q_INVOKABLE void changePassword(const QString &walletId
      , bs::wallet::QPasswordData *oldPasswordData, bs::wallet::QPasswordData *newPasswordData
      , const QJSValue &jsCallback);

   Q_INVOKABLE void addEidDevice(const QString &walletId
      , bs::wallet::QPasswordData *oldPasswordData, bs::wallet::QPasswordData *newPasswordData
      , const QJSValue &jsCallback);

   Q_INVOKABLE void removeEidDevice(const QString &walletId
      , bs::wallet::QPasswordData *oldPasswordData
      , int removedIndex, const QJSValue &jsCallback);

   Q_INVOKABLE QString getWoWalletFile(const QString &walletId) const;
   Q_INVOKABLE void importWoWallet(const QString &pathName, const QJSValue &jsCallback);
   Q_INVOKABLE void importHwWallet(HwWalletWrapper walletInfo, const QJSValue &jsCallback);

   Q_INVOKABLE void exportWatchingOnly(const QString &walletId
      , const QString &filePath, const QJSValue &jsCallback);

   Q_INVOKABLE bool backupPrivateKey(const QString &walletId
      , QString fileName, bool isPrintable, bs::wallet::QPasswordData *passwordData
      , const QJSValue &jsCallback);

   Q_INVOKABLE void signOfflineTx(const QString& fileName, const QJSValue &jsCallback);

   Q_INVOKABLE QString getRootWalletId(const QString &walletId) const;
   Q_INVOKABLE QString getRootWalletName(const QString &walletId) const;

   Q_INVOKABLE int indexOfWalletId(const QString &walletId) const;
   Q_INVOKABLE QString walletIdForIndex(int) const;
   Q_INVOKABLE QString walletIdForName(const QString &name) const;

   Q_INVOKABLE bool walletNameExists(const QString& name) const;
   Q_INVOKABLE QString generateNextWalletName() const;

   Q_INVOKABLE bool isWatchingOnlyWallet(const QString& walletId) const;

   bool walletsLoaded() const { return walletsSynchronized_; }

   QString defaultBackupLocation() const;

   Q_INVOKABLE void sendControlPassword(bs::wallet::QPasswordData *password);
   Q_INVOKABLE void changeControlPassword(bs::wallet::QPasswordData *oldPassword, bs::wallet::QPasswordData *newPassword
      , const QJSValue &jsCallback);

   Q_INVOKABLE QPixmap getQRCode(const QString& data, int size = 0) const;

   Q_INVOKABLE QString pixmapToDataUrl(const QPixmap &pixmap) const;

signals:
   void walletsChanged();
   void ccInfoChanged();

private slots:
   void onWalletsChanged();
   void setWalletsManager();

private:
   bool primaryWalletExists() const;
   std::shared_ptr<bs::sync::hd::Wallet> getRootForId(const QString &walletId) const;
   QStringList walletNames() const;
   QStringList priWalletNames() const;
   Q_INVOKABLE QJSValue invokeJsCallBack(QJSValue jsCallback, QJSValueList args);
   std::shared_ptr<bs::sync::hd::Wallet> getWoSyncWallet(const bs::sync::WatchingOnlyWallet &) const;
   bool hasCCInfo() const { return hasCCInfo_; }
   std::shared_ptr<SignAdapterContainer> signContainer() const;
   void signOfflineTxProceed(const QString& fileName, const std::vector<bs::core::wallet::TXSignRequest> &parsedReqs, const QJSValue &jsCallback);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   SignerAdapter  *  adapter_{};
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<SignAdapterContainer> signContainer_;
   bool walletsSynchronized_ = false;
   bool hasCCInfo_ = false;

   std::function<void(bs::error::ErrorCode result)> createChangePwdResultCb(const QString &walletId, const QJSValue &jsCallback);
};

#endif // __WALLETS_PROXY_H__
