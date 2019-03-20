#ifndef __QML_APP_H__
#define __QML_APP_H__

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
}
namespace spdlog {
   class logger;
}
class OfflineProcessor;
class QmlWalletsViewModel;
class QQmlContext;
class QMLStatusUpdater;
class QSystemTrayIcon;
class SignerAdapter;
class SignerSettings;
class WalletsProxy;
class ZmqSecuredServerConnection;
class DBusNotification;
class QmlFactory;

class QMLAppObj : public QObject
{
   Q_OBJECT

public:
   QMLAppObj(SignerAdapter *, const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<SignerSettings> &, QQmlContext *);

   void Start();
   void SetRootObject(QObject *);

signals:
   void loadingComplete();
   void cancelSignTx(const QString &txId);

private slots:
   void onReady();
   void onWalletsSynced();
   void onPasswordAccepted(const QString &walletId
                           , bs::wallet::QPasswordData *passwordData
                           , bool cancelledByUser);
   void onOfflinePassword(const bs::core::wallet::TXSignRequest &);
   void onPasswordRequested(const bs::core::wallet::TXSignRequest &, const QString &prompt);
   void onAutoSignPwdRequested(const std::string &walletId);
   void onOfflineChanged();
   void onWalletsDirChanged();
   void onListenSocketChanged();
   void onLimitsChanged();
   void onSysTrayMsgClicked();
   void onSysTrayActivated(QSystemTrayIcon::ActivationReason reason);
   void onCancelSignTx(const BinaryData &txId);

private:
   void settingsConnections();
   void requestPassword(const bs::core::wallet::TXSignRequest &, const QString &prompt, bool alert = true);

   void initZmqKeys();
   void registerQtTypes();

   SignerAdapter  *  adapter_;
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<SignerSettings>  settings_;
   QQmlContext                *     ctxt_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsMgr_;
   std::shared_ptr<ZmqBIP15XServerConnection>   connection_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
   std::shared_ptr<QMLStatusUpdater>            statusUpdater_;
   std::shared_ptr<WalletsProxy>                walletsProxy_;
   std::shared_ptr<QmlFactory>                  qmlFactory_;
   QObject  *  rootObj_ = nullptr;
   QmlWalletsViewModel  *  walletsModel_ = nullptr;
   QSystemTrayIcon      *  trayIcon_ = nullptr;
   SecureBinaryData                             zmqPubKey_;
   SecureBinaryData                             zmqPrvKey_;

   enum NotificationMode {
      QSystemTray,
      Freedesktop
   };

   NotificationMode notifMode_;

#ifdef BS_USE_DBUS
   DBusNotification *dbus_;
#endif // BS_USE_DBUS

   std::unordered_set<std::string>  offlinePasswordRequests_;
};

#endif // __QML_APP_H__
