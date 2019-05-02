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

   bs::signer::RequestId send(signer::PacketType pt, const std::string &data);
   std::shared_ptr<ZmqBIP15XDataConnection> getDataConnection() { return connection_; }

   void setTxSignCb(bs::signer::RequestId reqId, const std::function<void(const BinaryData &)> &cb) {
      cbSignReqs_[reqId] = cb;
   }
   void setWalleteInfoCb(bs::signer::RequestId reqId
      , const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb) {
      cbWalletInfo_[reqId] = cb;
   }
   void setHDWalletDataCb(bs::signer::RequestId reqId, const std::function<void(bs::sync::HDWalletData)> &cb) {
      cbHDWalletData_[reqId] = cb;
   }
   void setWalletDataCb(bs::signer::RequestId reqId, const std::function<void(bs::sync::WalletData)> &cb) {
      cbWalletData_[reqId] = cb;
   }
   void setWatchOnlyCb(bs::signer::RequestId reqId, const std::function<void(const bs::sync::WatchingOnlyWallet &)> &cb) {
      cbWO_[reqId] = cb;
   }
   void setDecryptNodeCb(bs::signer::RequestId reqId
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb) {
      cbDecryptNode_[reqId] = cb;
   }
   void setReloadWalletsCb(bs::signer::RequestId reqId, const std::function<void()> &cb) {
      cbReloadWallets_[reqId] = cb;
   }
   void setChangePwCb(bs::signer::RequestId reqId, const std::function<void(bool)> &cb) {
      cbChangePwReqs_[reqId] = cb;
   }
   void setCreateHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bool success, const std::string& errorMsg)> &cb) {
      cbCreateHDWalletReqs_[reqId] = cb;
   }
   void setDeleteHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bool success, const std::string& errorMsg)> &cb) {
      cbDeleteHDWalletReqs_[reqId] = cb;
   }
   void setHeadlessPubKeyCb(bs::signer::RequestId reqId, const std::function<void(const std::string &pubKey)> &cb) {
      cbHeadlessPubKeyReqs_[reqId] = cb;
   }

private:
   void onReady(const std::string &data);
   void onPeerConnected(const std::string &data, bool connected);
   void onPasswordRequested(const std::string &data);
   void onTxSigned(const std::string &data, bs::signer::RequestId);
   void onXbtSpent(const std::string &data);
   void onAutoSignActivate(const std::string &data);
   void onSyncWalletInfo(const std::string &data, bs::signer::RequestId);
   void onSyncHDWallet(const std::string &data, bs::signer::RequestId);
   void onSyncWallet(const std::string &data, bs::signer::RequestId);
   void onCreateWO(const std::string &data, bs::signer::RequestId);
   void onDecryptedKey(const std::string &data, bs::signer::RequestId);
   void onReloadWallets(bs::signer::RequestId);
   void onExecCustomDialog(const std::string &data, bs::signer::RequestId);
   void onChangePassword(const std::string &data, bs::signer::RequestId);
   void onCreateHDWallet(const std::string &data, bs::signer::RequestId);
   void onDeleteHDWallet(const std::string &data, bs::signer::RequestId);
   void onHeadlessPubKey(const std::string &data, bs::signer::RequestId);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqBIP15XDataConnection>  connection_;
   SignerAdapter  *  parent_;
   bs::signer::RequestId   seq_ = 1;
   std::map<bs::signer::RequestId, std::function<void(const BinaryData &)>>      cbSignReqs_;
   std::map<bs::signer::RequestId, std::function<void(std::vector<bs::sync::WalletInfo>)>>  cbWalletInfo_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::HDWalletData)>>  cbHDWalletData_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::WalletData)>>    cbWalletData_;
   std::map<bs::signer::RequestId, std::function<void(const bs::sync::WatchingOnlyWallet &)>>   cbWO_;
   std::map<bs::signer::RequestId
      , std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)>>   cbDecryptNode_;
   std::map<bs::signer::RequestId, std::function<void()>>   cbReloadWallets_;
   std::map<bs::signer::RequestId, std::function<void(bool success)>> cbChangePwReqs_;
   std::map<bs::signer::RequestId, std::function<void(bool success, const std::string& errorMsg)>> cbCreateHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(bool success, const std::string& errorMsg)>> cbDeleteHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(const std::string &pubKey)>> cbHeadlessPubKeyReqs_;
};


#endif // SIGNER_INTERFACE_LISTENER_H
