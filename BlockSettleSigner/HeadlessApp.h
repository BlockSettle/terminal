#ifndef __HEADLESS_APP_H__
#define __HEADLESS_APP_H__

#include <memory>
#include <QObject>
#include "EncryptionUtils.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
}
class HeadlessContainerListener;
class OfflineProcessor;
class SignerSettings;
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

private:
   void OnlineProcessing();
   void OfflineProcessing();

   void setConsoleEcho(bool enable) const;

   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<SignerSettings>        settings_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;
   std::shared_ptr<ZmqSecuredServerConnection>  connection_;
   std::shared_ptr<HeadlessContainerListener>   listener_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
   SecureBinaryData                             zmqPubKey_;
   SecureBinaryData                             zmqPrvKey_;
};

#endif // __HEADLESS_APP_H__
