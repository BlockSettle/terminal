#ifndef SIGNER_ADAPTER_LISTENER_H
#define SIGNER_ADAPTER_LISTENER_H

#include <memory>
#include "CoreWallet.h"
#include "SignerDefs.h"
#include "ServerConnectionListener.h"

#include "bs_signer.pb.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
}
class HeadlessAppObj;
class HeadlessContainerListener;
class ServerConnection;


class SignerAdapterListener : public ServerConnectionListener
{
public:
   SignerAdapterListener(HeadlessAppObj *, const std::shared_ptr<ServerConnection> &
      , const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<bs::core::WalletsManager> &);
   ~SignerAdapterListener() noexcept override;

   bool onReady(int cur = 0, int total = 0);

protected:
   void OnDataFromClient(const std::string &clientId, const std::string &data) override;
   void OnClientConnected(const std::string &clientId) override;
   void OnClientDisconnected(const std::string &clientId) override;

private:
   void setCallbacks();
   bool sendData(Blocksettle::Communication::signer::PacketType, const std::string &data
      , bs::signer::RequestId reqId = 0);

   bool onSignTxReq(const std::string &data, bs::signer::RequestId);
   bool onSyncWalletInfo(bs::signer::RequestId);
   bool onSyncHDWallet(const std::string &data, bs::signer::RequestId);
   bool onSyncWallet(const std::string &data, bs::signer::RequestId);
   bool onCreateWO(const std::string &data, bs::signer::RequestId);
   bool onGetDecryptedNode(const std::string &data, bs::signer::RequestId);
   bool onSetLimits(const std::string &data);
   bool onPasswordReceived(const std::string &data);
   bool onRequestClose();
   bool onReloadWallets(const std::string &data, bs::signer::RequestId);
   bool onReconnect(const std::string &data);
   bool onAutoSignRequest(const std::string &data);
   bool onChangePassword(const std::string &data, bs::signer::RequestId);

private:
   HeadlessAppObj *  app_;
   std::shared_ptr<ServerConnection>   connection_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;
   bool  ready_ = false;
};

#endif // SIGNER_ADAPTER_LISTENER_H
