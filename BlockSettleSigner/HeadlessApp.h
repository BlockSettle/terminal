#ifndef __HEADLESS_APP_H__
#define __HEADLESS_APP_H__

#include <memory>
#include <QObject>
#include "EncryptionUtils.h"
#include "HeadlessContainer.h"

namespace spdlog {
   class logger;
}
class HeadlessContainerListener;
class OfflineProcessor;
class SignerSettings;
class WalletsManager;
class ZmqSecuredServerConnection;


class HeadlessAppObj : public QObject
{
   Q_OBJECT

public:
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<SignerSettings> &);

   void Start();

signals:
   void finished();

/*protected slots:
   void onAuthenticated();
   void onConnected();
   void onDisconnected();
   void onConnError(const QString &err);
   void onPacketReceived(Blocksettle::Communication::headless::RequestPacket);*/

private:
   void OnlineProcessing();
   void OfflineProcessing();

   void setConsoleEcho(bool enable) const;

   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<SignerSettings>        settings_;
   std::shared_ptr<WalletsManager>              walletsMgr_;
   std::shared_ptr<ZmqSecuredDataConnection>    connection_;
   std::shared_ptr<HeadlessListener>            listener_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
   SecureBinaryData                             zmqPubKey_;
   SecureBinaryData                             zmqPrvKey_;
};

#endif // __HEADLESS_APP_H__
