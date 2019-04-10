#ifndef SIGNER_INTERFACE_LISTENER_H
#define SIGNER_INTERFACE_LISTENER_H

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "SignContainer.h"
#include "DataConnectionListener.h"
#include "bs_signer.pb.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
namespace spdlog {
   class logger;
}
class ZmqBIP15XDataConnection;
class SignerAdapter;

using namespace Blocksettle::Communication;

class SignerInterfaceListener : public DataConnectionListener
{
public:
   SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ZmqBIP15XDataConnection> &conn, SignerAdapter *parent);

   void OnDataReceived(const std::string &) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   SignContainer::RequestId send(signer::PacketType pt, const std::string &data);

   void setTxSignCb(SignContainer::RequestId reqId, const std::function<void(const BinaryData &)> &cb) {
      cbSignReqs_[reqId] = cb;
   }
   void setWalleteInfoCb(SignContainer::RequestId reqId
      , const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb) {
      cbWalletInfo_[reqId] = cb;
   }
   void setHDWalletDataCb(SignContainer::RequestId reqId, const std::function<void(bs::sync::HDWalletData)> &cb) {
      cbHDWalletData_[reqId] = cb;
   }
   void setWalletDataCb(SignContainer::RequestId reqId, const std::function<void(bs::sync::WalletData)> &cb) {
      cbWalletData_[reqId] = cb;
   }
   void setWatchOnlyCb(SignContainer::RequestId reqId, const std::function<void(const bs::sync::WatchingOnlyWallet &)> &cb) {
      cbWO_[reqId] = cb;
   }
   void setDecryptNodeCb(SignContainer::RequestId reqId
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb) {
      cbDecryptNode_[reqId] = cb;
   }
   void setReloadWalletsCb(SignContainer::RequestId reqId, const std::function<void()> &cb) {
      cbReloadWallets_[reqId] = cb;
   }
   void setChangePwCb(SignContainer::RequestId reqId, const std::function<void(bool)> &cb) {
      cbChangePwReqs_[reqId] = cb;
   }

private:
   void onReady(const std::string &data);
   void onPeerConnected(const std::string &data, bool connected);
   void onPasswordRequested(const std::string &data);
   void onTxSigned(const std::string &data, SignContainer::RequestId);
   void onXbtSpent(const std::string &data);
   void onAutoSignActivate(const std::string &data);
   void onSyncWalletInfo(const std::string &data, SignContainer::RequestId);
   void onSyncHDWallet(const std::string &data, SignContainer::RequestId);
   void onSyncWallet(const std::string &data, SignContainer::RequestId);
   void onCreateWO(const std::string &data, SignContainer::RequestId);
   void onDecryptedKey(const std::string &data, SignContainer::RequestId);
   void onReloadWallets(SignContainer::RequestId);
   void onExecCustomDialog(const std::string &data, SignContainer::RequestId);
   void onChangePassword(const std::string &data, SignContainer::RequestId);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqBIP15XDataConnection>  connection_;
   SignerAdapter  *  parent_;
   SignContainer::RequestId   seq_ = 1;
   std::map<SignContainer::RequestId, std::function<void(const BinaryData &)>>      cbSignReqs_;
   std::map<SignContainer::RequestId, std::function<void(std::vector<bs::sync::WalletInfo>)>>  cbWalletInfo_;
   std::map<SignContainer::RequestId, std::function<void(bs::sync::HDWalletData)>>  cbHDWalletData_;
   std::map<SignContainer::RequestId, std::function<void(bs::sync::WalletData)>>    cbWalletData_;
   std::map<SignContainer::RequestId, std::function<void(const bs::sync::WatchingOnlyWallet &)>>   cbWO_;
   std::map<SignContainer::RequestId
      , std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)>>   cbDecryptNode_;
   std::map<SignContainer::RequestId, std::function<void()>>   cbReloadWallets_;
   std::map<SignContainer::RequestId, std::function<void(bool success)>> cbChangePwReqs_;
};


#endif // SIGNER_INTERFACE_LISTENER_H
