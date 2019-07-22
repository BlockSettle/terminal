#ifndef SIGNER_INTERFACE_LISTENER_H
#define SIGNER_INTERFACE_LISTENER_H

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "SignContainer.h"
#include "DataConnectionListener.h"
#include "QmlBridge.h"
#include "QmlFactory.h"
#include "TXInfo.h"

#include "bs_signer.pb.h"

#include <functional>
#include <memory>

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


class SignerInterfaceListener : public QObject, public DataConnectionListener
{
   Q_OBJECT

public:
   SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<QmlBridge> &qmlBridge
      , const std::shared_ptr<ZmqBIP15XDataConnection> &conn, SignerAdapter *parent);

   void OnDataReceived(const std::string &) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   bs::signer::RequestId send(signer::PacketType pt, const std::string &data);
   std::shared_ptr<ZmqBIP15XDataConnection> getDataConnection() { return connection_; }

   void setTxSignCb(bs::signer::RequestId reqId, const std::function<void(bs::error::ErrorCode result, const BinaryData &)> &cb) {
      cbSignReqs_[reqId] = cb;
   }
   void setWalletInfoCb(bs::signer::RequestId reqId
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
   void setCreateHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bs::error::ErrorCode errorCode)> &cb) {
      cbCreateHDWalletReqs_[reqId] = cb;
   }
   void setDeleteHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bool success, const std::string& errorMsg)> &cb) {
      cbDeleteHDWalletReqs_[reqId] = cb;
   }
   void setAutoSignCb(bs::signer::RequestId reqId, const std::function<void(bs::error::ErrorCode errorCode)> &cb) {
      cbAutoSignReqs_[reqId] = cb;
   }

   void setQmlFactory(const std::shared_ptr<QmlFactory> &qmlFactory);

private:
   void processData(const std::string &);

   void onReady(const std::string &data);
   void onPeerConnected(const std::string &data, bool connected);
   void onDecryptWalletRequested(const std::string &data);
   void onTxSigned(const std::string &data, bs::signer::RequestId);
   void onCancelTx(const std::string &data, bs::signer::RequestId);
   void onXbtSpent(const std::string &data);
   void onAutoSignActivated(const std::string &data, bs::signer::RequestId);
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
   void onUpdateStatus(const std::string &data);
   void onTerminalHandshakeFailed(const std::string &data);

   void requestPasswordForTx(signer::PasswordDialogType reqType, bs::sync::PasswordDialogData *dialogData
      , bs::wallet::TXInfo *txInfo, bs::hd::WalletInfo *walletInfo);
   void requestPasswordForSettlementTx(signer::PasswordDialogType reqType, bs::sync::PasswordDialogData *dialogData
      , bs::wallet::TXInfo *txInfo, bs::hd::WalletInfo *walletInfo);
   void requestPasswordForAuthLeaf(bs::sync::PasswordDialogData *dialogData);

   void shutdown();
   bs::signer::QmlCallbackBase *createQmlPasswordCallback();

private:
   std::shared_ptr<spdlog::logger>           logger_;
   std::shared_ptr<ZmqBIP15XDataConnection>  connection_;
   std::shared_ptr<QmlFactory>               qmlFactory_;
   SignerAdapter                             * parent_;

   bs::signer::RequestId seq_ = 1;
   std::map<bs::signer::RequestId, std::function<void(bs::error::ErrorCode errorCode, const BinaryData &)>> cbSignReqs_;
   std::map<bs::signer::RequestId, std::function<void(std::vector<bs::sync::WalletInfo>)>>  cbWalletInfo_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::HDWalletData)>>  cbHDWalletData_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::WalletData)>>    cbWalletData_;
   std::map<bs::signer::RequestId, std::function<void(const bs::sync::WatchingOnlyWallet &)>>   cbWO_;
   std::map<bs::signer::RequestId
      , std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)>>   cbDecryptNode_;
   std::map<bs::signer::RequestId, std::function<void()>>   cbReloadWallets_;
   std::map<bs::signer::RequestId, std::function<void(bool success)>> cbChangePwReqs_;
   std::map<bs::signer::RequestId, std::function<void(bs::error::ErrorCode errorCode)>> cbCreateHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(bool success, const std::string& errorMsg)>> cbDeleteHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(bs::error::ErrorCode errorCode)>> cbAutoSignReqs_;

   std::shared_ptr<QmlBridge>  qmlBridge_;

};


#endif // SIGNER_INTERFACE_LISTENER_H
