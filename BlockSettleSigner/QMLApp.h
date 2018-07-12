#ifndef __QML_APP_H__
#define __QML_APP_H__

#include <memory>
#include <unordered_set>
#include <QObject>
#include "MetaData.h"


namespace spdlog {
   class logger;
}
class FrejaProxy;
class HeadlessContainerListener;
class OfflineProcessor;
class QmlWalletsViewModel;
class QQmlContext;
class QMLStatusUpdater;
class QSystemTrayIcon;
class SignerSettings;
class WalletsManager;
class WalletsProxy;
class ZmqSecuredServerConnection;


class QMLAppObj : public QObject
{
   Q_OBJECT

public:
   QMLAppObj(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<SignerSettings> &, QQmlContext *);

   void Start();
   void SetRootObject(QObject *);

signals:
   void loadingComplete();

private slots:
   void onPasswordAccepted(const QString &walletId, const QString &password);
   void onOfflinePassword(const bs::wallet::TXSignRequest &);
   void onPasswordRequested(const bs::wallet::TXSignRequest &, const QString &prompt);
   void onAutoSignPwdRequested(const std::string &walletId);
   void onOfflineChanged();
   void onWalletsDirChanged();
   void onListenSocketChanged();
   void onLimitsChanged();
   void onSysTrayMsgClicked();

private:
   void OnlineProcessing();
   void walletsLoad();
   void settingsConnections();
   void requestPassword(const bs::wallet::TXSignRequest &, const QString &prompt, bool alert = true);
   void disconnect();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<SignerSettings>  params_;
   QQmlContext                *     ctxt_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   std::shared_ptr<ZmqSecuredServerConnection>  connection_;
   std::shared_ptr<HeadlessContainerListener>   listener_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
   std::shared_ptr<QMLStatusUpdater>            statusUpdater_;
   std::shared_ptr<WalletsProxy>                walletsProxy_;
   std::shared_ptr<FrejaProxy>                  frejaProxy_;
   QObject  *  rootObj_ = nullptr;
   QmlWalletsViewModel  *  walletsModel_ = nullptr;
   QSystemTrayIcon      *  trayIcon_ = nullptr;
   std::unordered_set<std::string>  offlinePasswordRequests_;
};

#endif // __QML_APP_H__
