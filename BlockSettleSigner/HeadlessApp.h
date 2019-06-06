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
namespace Blocksettle {
   namespace Communication {
      namespace signer {
         class Settings;
      }
   }
}
class HeadlessContainerListener;
class SignerAdapterListener;
class HeadlessSettings;
class OfflineProcessor;
class SignerSettings;
class ZmqBIP15XServerConnection;
class HeadlessContainerCallbacks;
class DispatchQueue;

class HeadlessAppObj
{
public:
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<HeadlessSettings> &
      , const std::shared_ptr<DispatchQueue>&);

   ~HeadlessAppObj() noexcept;

   void start();
   void stop();
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

   void updateSettings(const std::unique_ptr<Blocksettle::Communication::signer::Settings> &);

   ZmqBIP15XServerConnection* connection() const;
   bs::signer::BindStatus signerBindStatus() const { return signerBindStatus_; }

private:
   void startInterface();
   void onlineProcessing();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<HeadlessSettings>      settings_;
   const std::shared_ptr<DispatchQueue>         queue_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;

   // Declare listeners before connections (they should be destroyed after)
   std::unique_ptr<HeadlessContainerListener>   terminalListener_;
   std::unique_ptr<SignerAdapterListener>       guiListener_;

   std::unique_ptr<ZmqBIP15XServerConnection>   terminalConnection_;
   std::unique_ptr<ZmqBIP15XServerConnection>   guiConnection_;

   ProcessControl             guiProcess_;
   std::function<void(bool)>  cbReady_;
   bool ready_{false};
   std::atomic<bs::signer::BindStatus> signerBindStatus_{bs::signer::BindStatus::Inactive};
};

#endif // __HEADLESS_APP_H__
