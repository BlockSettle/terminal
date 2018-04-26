#ifndef __HEADLESS_APP_H__
#define __HEADLESS_APP_H__

#include <memory>
#include <QObject>


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
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<SignerSettings> &);

   void Start();

signals:
   void finished();

private:
   void OnlineProcessing();
   void OfflineProcessing();

   void setConsoleEcho(bool enable) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<SignerSettings>        params_;
   std::shared_ptr<WalletsManager>              walletsMgr_;
   std::shared_ptr<ZmqSecuredServerConnection>  connection_;
   std::shared_ptr<HeadlessContainerListener>   listener_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
};

#endif // __HEADLESS_APP_H__
