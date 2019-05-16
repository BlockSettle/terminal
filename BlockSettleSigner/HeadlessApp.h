#ifndef __HEADLESS_APP_H__
#define __HEADLESS_APP_H__

#include <atomic>
#include <functional>
#include <memory>
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "ProcessControl.h"
#include "SignerDefs.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
}
class HeadlessContainerListener;
class SignerAdapterListener;
class HeadlessSettings;
class OfflineProcessor;
class SignerSettings;
class ZmqBIP15XServerConnection;
class HeadlessContainerCallbacks;

class HeadlessAppObj
{
public:
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<HeadlessSettings> &);

   ~HeadlessAppObj() noexcept = default;

   void start();
   void setReadyCallback(const std::function<void(bool)> &cb) { cbReady_ = cb; }
   void setCallbacks(HeadlessContainerCallbacks *callbacks);

   void reloadWallets(const std::string &, const std::function<void()> &);
   void reconnect(const std::string &listenAddr, const std::string &port);
   void setOnline(bool);
   void setLimits(bs::signer::Limits);
   void passwordReceived(const std::string &walletId, const SecureBinaryData &, bool cancelledByUser);
   void deactivateAutoSign();
   void addPendingAutoSignReq(const std::string &walletId);
   void close();
   void walletsListUpdated();

   std::shared_ptr<ZmqBIP15XServerConnection> connection() const;
   bool headlessBindFailed() const { return headlessBindFailed_; }

private:
   void startInterface();
   void onlineProcessing();

private:

   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<HeadlessSettings>      settings_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;
   std::shared_ptr<ZmqBIP15XServerConnection>   connection_;
   std::shared_ptr<HeadlessContainerListener>   listener_;
   std::shared_ptr<SignerAdapterListener>       adapterLsn_;
   ProcessControl             guiProcess_;
   std::function<void(bool)>  cbReady_;
   bool ready_{false};
   std::atomic_bool headlessBindFailed_{false};
};

#endif // __HEADLESS_APP_H__
