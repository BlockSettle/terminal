/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __QML_APP_H__
#define __QML_APP_H__

#include <memory>
#include <unordered_set>
#include <QObject>
#include <QSystemTrayIcon>
#include "Wallets/SignerDefs.h"

namespace bs {
   namespace wallet {
      class QPasswordData;
   }
   namespace sync {
      class WalletsManager;
   }
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
}
namespace spdlog {
   class logger;
}

class DBusNotification;
class QmlFactory;
class QMLStatusUpdater;
class QmlWalletsViewModel;
class QQmlContext;
class QSplashScreen;
class QSystemTrayIcon;
class SignerAdapter;
class SignerSettings;
class WalletsProxy;
class HwDeviceManager;

class QMLAppObj : public QObject
{
   Q_OBJECT

public:
   QMLAppObj(SignerAdapter *, const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<SignerSettings> &, QSplashScreen *, QQmlContext *);

   void Start();
   void SetRootObject(QObject *);

   void raiseQmlWindow();
   void hideQmlWindow();

   Q_INVOKABLE QString getUrlPath(const QUrl &url);
   Q_INVOKABLE QString getUrlPathWithoutExtention(const QUrl &url);

signals:
   void cancelSignTx(const QString &txId);
   void qmlAppStarted();

private slots:
   void onReady();
   void onConnectionError();
   void onHeadlessBindUpdated(bs::signer::BindStatus status);
   void onWalletsSynced();
   void onLimitsChanged();
   void onSettingChanged(int);
   void onSysTrayMsgClicked();
   void onSysTrayActivated(QSystemTrayIcon::ActivationReason reason);
   void onCancelSignTx(const BinaryData &txId);
   void onCustomDialogRequest(const QString &dialogName, const QVariantMap &data);
   void onTerminalHandshakeFailed(const std::string &peerAddress);
   void onSignerPubKeyUpdated(const BinaryData &pubKey);

   void showTrayNotify(const QString &title, const QString &msg);
private:
   void settingsConnections();
   void registerQtTypes();

   SignerAdapter  *  adapter_{};
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<SignerSettings>  settings_;
   QSplashScreen              *     splashScreen_ = nullptr;
   QQmlContext                *     ctxt_ = nullptr;
   std::shared_ptr<bs::sync::WalletsManager>    walletsMgr_;
   std::shared_ptr<QMLStatusUpdater>            statusUpdater_;
   std::shared_ptr<WalletsProxy>                walletsProxy_;
   std::shared_ptr<QmlFactory>                  qmlFactory_;
   HwDeviceManager* hwDeviceManager_{};
   QObject  *  rootObj_ = nullptr;
   QmlWalletsViewModel  *  walletsModel_ = nullptr;
   // Tray icon will not be started with light UI signer
   QSystemTrayIcon      *  trayIconOptional_ = nullptr;

   enum NotificationMode {
      QSystemTray,
      Freedesktop
   };

   NotificationMode notifMode_;

#ifdef BS_USE_DBUS
   DBusNotification *dbus_;
#endif // BS_USE_DBUS

   std::unordered_set<std::string> lastFailedTerminals_;
};

#endif // __QML_APP_H__
