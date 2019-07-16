#ifndef __QML_APP_H__
#define __QML_APP_H__

#include "SecureBinaryData.h"

#include <memory>
#include <unordered_set>
#include <QObject>
#include <QSystemTrayIcon>

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
class OfflineProcessor;
class QmlFactory;
class QMLStatusUpdater;
class QmlWalletsViewModel;
class QQmlContext;
class QSplashScreen;
class QSystemTrayIcon;
class SignerAdapter;
class SignerSettings;
class WalletsProxy;

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

signals:
   void cancelSignTx(const QString &txId);

private slots:
   void onReady();
   void onConnectionError();
   void onHeadlessBindUpdated(bool success);
   void onWalletsSynced();
   void onPasswordAccepted(const QString &walletId
                           , bs::wallet::QPasswordData *passwordData
                           , bool cancelledByUser);
   void onOfflinePassword(const bs::core::wallet::TXSignRequest &);
   void onLimitsChanged();
   void onSettingChanged(int);
   void onSysTrayMsgClicked();
   void onSysTrayActivated(QSystemTrayIcon::ActivationReason reason);
   void onCancelSignTx(const BinaryData &txId);
   void onCustomDialogRequest(const QString &dialogName, const QVariantMap &data);
   void onTerminalHandshakeFailed(const std::string &peerAddress);

   void showTrayNotify(const QString &title, const QString &msg);
private:
   void settingsConnections();
   void registerQtTypes();

   SignerAdapter  *  adapter_;
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<SignerSettings>  settings_;
   QSplashScreen              *     splashScreen_ = nullptr;
   QQmlContext                *     ctxt_ = nullptr;
   std::shared_ptr<bs::sync::WalletsManager>    walletsMgr_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
   std::shared_ptr<QMLStatusUpdater>            statusUpdater_;
   std::shared_ptr<WalletsProxy>                walletsProxy_;
   std::shared_ptr<QmlFactory>                  qmlFactory_;
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

   std::unordered_set<std::string> offlinePasswordRequests_;
   std::unordered_set<std::string> lastFailedTerminals_;
};

#endif // __QML_APP_H__
