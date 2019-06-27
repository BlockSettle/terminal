#ifndef SIGNER_ADAPTER_LISTENER_H
#define SIGNER_ADAPTER_LISTENER_H

#include <memory>
#include "CoreWallet.h"
#include "SignerDefs.h"
#include "ServerConnectionListener.h"
#include "ZMQ_BIP15X_ServerConnection.h"

#include "bs_signer.pb.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace hd {
         class Wallet;
      }
      class WalletsManager;
   }
}
class DispatchQueue;
class HeadlessAppObj;
class HeadlessContainerCallbacks;
class HeadlessContainerCallbacksImpl;
class HeadlessSettings;
class ServerConnection;

class SignerAdapterListener : public ServerConnectionListener
{
public:
   SignerAdapterListener(HeadlessAppObj *app
      , ZmqBIP15XServerConnection *connection
      , const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::core::WalletsManager> &walletsMgr
      , const std::shared_ptr<DispatchQueue> &queue
      , const std::shared_ptr<HeadlessSettings> &settings);
   ~SignerAdapterListener() noexcept override;

   ZmqBIP15XServerConnection *getServerConn() const { return connection_; }

   // Sent to GUI status update message
   void sendStatusUpdate();

   void resetConnection();

   HeadlessContainerCallbacks *callbacks() const;
protected:
   void OnDataFromClient(const std::string &clientId, const std::string &data) override;
   void OnClientConnected(const std::string &clientId) override;
   void OnClientDisconnected(const std::string &clientId) override;
   void onClientError(const std::string& clientId, const std::string &error) override;

   void processData(const std::string &clientId, const std::string &data);

   bool sendData(Blocksettle::Communication::signer::PacketType, const std::string &data
      , bs::signer::RequestId reqId = 0);
   bool sendWoWallet(const std::shared_ptr<bs::core::hd::Wallet> &
      , Blocksettle::Communication::signer::PacketType, bs::signer::RequestId reqId = 0);

   bool onSignOfflineTxRequest(const std::string &data, bs::signer::RequestId);
   bool onSyncWalletInfo(bs::signer::RequestId);
   bool onSyncHDWallet(const std::string &data, bs::signer::RequestId);
   bool onSyncWallet(const std::string &data, bs::signer::RequestId);
   bool onCreateWO(const std::string &data, bs::signer::RequestId);
   bool onGetDecryptedNode(const std::string &data, bs::signer::RequestId);
   bool onSetLimits(const std::string &data);
   bool onPasswordReceived(const std::string &data);
   bool onRequestClose();
   bool onReloadWallets(const std::string &data, bs::signer::RequestId);
   bool onAutoSignRequest(const std::string &data, bs::signer::RequestId);
   bool onChangePassword(const std::string &data, bs::signer::RequestId);
   bool onCreateHDWallet(const std::string &data, bs::signer::RequestId);
   bool onDeleteHDWallet(const std::string &data, bs::signer::RequestId);
   bool onHeadlessPubKeyRequest(const std::string &data, bs::signer::RequestId);
   bool onImportWoWallet(const std::string &data, bs::signer::RequestId);
   bool onSyncSettings(const std::string &data);

   void walletsListUpdated();
   void shutdownIfNeeded();

   bool sendReady();
private:
   friend class HeadlessContainerCallbacksImpl;

   HeadlessAppObj *  app_;
   ZmqBIP15XServerConnection *connection_{};
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;
   std::shared_ptr<DispatchQueue> queue_;
   std::shared_ptr<HeadlessSettings>   settings_;
   std::unique_ptr<HeadlessContainerCallbacksImpl> callbacks_;
};

#endif // SIGNER_ADAPTER_LISTENER_H
