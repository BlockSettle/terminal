/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNER_INTERFACE_LISTENER_H
#define SIGNER_INTERFACE_LISTENER_H

#include <QObject>
#include "DataConnectionListener.h"
#include "SignerUiDefs.h"
#include "TXInfo.h"

#include "bs_signer.pb.h"

#include <functional>
#include <memory>
#include <queue>

namespace bs {
   namespace signer {
      class QmlCallbackBase;
   }
}
namespace spdlog {
   class logger;
}

class DataConnection;
class SignerAdapter;
class QmlBridge;
class QmlFactory;

using namespace Blocksettle::Communication;


class SignerInterfaceListener : public QObject, public DataConnectionListener
{
   Q_OBJECT

public:
   SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<QmlBridge> &qmlBridge
      , const std::shared_ptr<DataConnection> &conn, SignerAdapter *parent);

   void OnDataReceived(const std::string &) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   bs::signer::RequestId send(signer::PacketType pt, const std::string &data);
   std::shared_ptr<DataConnection> getDataConnection() { return connection_; }

   using BasicCb = std::function<void(bs::error::ErrorCode errorCode)>;

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
   void setExportWatchOnlyCb(bs::signer::RequestId reqId, const std::function<void(const BinaryData &)> &cb) {
      cbExportWO_[reqId] = cb;
   }
   void setDecryptNodeCb(bs::signer::RequestId reqId
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb) {
      cbDecryptNode_[reqId] = cb;
   }
   void setChangePwCb(bs::signer::RequestId reqId, const BasicCb &cb) {
      cbChangePwReqs_[reqId] = cb;
   }
   void setCreateHDWalletCb(bs::signer::RequestId reqId, const BasicCb &cb) {
      cbCreateHDWalletReqs_[reqId] = cb;
   }
   void setDeleteHDWalletCb(bs::signer::RequestId reqId, const std::function<void(bool success, const std::string& errorMsg)> &cb) {
      cbDeleteHDWalletReqs_[reqId] = cb;
   }
   void setAutoSignCb(bs::signer::RequestId reqId, const BasicCb &cb) {
      cbAutoSignReqs_[reqId] = cb;
   }
   void setChangeControlPwCb(bs::signer::RequestId reqId, const BasicCb &cb) {
      cbChangeControlPwReqs_[reqId] = cb;
   }
   void setVerifyOfflineTxRequestCb(bs::signer::RequestId reqId, const BasicCb &cb) {
      cbVerifyOfflineTxRequestReqs_[reqId] = cb;
   }

   void setQmlFactory(const std::shared_ptr<QmlFactory> &qmlFactory);

   void closeConnection();

public slots:
   void onWalletsSynchronizationStarted();
   void onWalletsSynchronized();

private:
   void processData(const std::string &);

   void onReady(const std::string &data);
   void onPeerConnected(const std::string &data);
   void onPeerDisconnected(const std::string &data);
   void onDecryptWalletRequested(const std::string &data);
   void onTxSigned(const std::string &data, bs::signer::RequestId);
   void onUpdateDialogData(const std::string &data, bs::signer::RequestId);
   void onCancelTx(const std::string &data, bs::signer::RequestId);
   void onXbtSpent(const std::string &data);
   void onAutoSignActivated(const std::string &data, bs::signer::RequestId);
   void onSyncWalletInfo(const std::string &data, bs::signer::RequestId);
   void onSyncHDWallet(const std::string &data, bs::signer::RequestId);
   void onSyncWallet(const std::string &data, bs::signer::RequestId);
   void onCreateWO(const std::string &data, bs::signer::RequestId);
   void onExportWO(const std::string &data, bs::signer::RequestId);
   void onDecryptedKey(const std::string &data, bs::signer::RequestId);
   void onExecCustomDialog(const std::string &data, bs::signer::RequestId);
   void onChangePassword(const std::string &data, bs::signer::RequestId);
   void onCreateHDWallet(const std::string &data, bs::signer::RequestId);
   void onDeleteHDWallet(const std::string &data, bs::signer::RequestId);
   void onUpdateWallet(const std::string &data, bs::signer::RequestId);
   void onUpdateStatus(const std::string &data);
   void onUpdateControlPasswordStatus(const std::string &data);
   void onTerminalEvent(const std::string &data);
   void onChangeControlPassword(const std::string &data, bs::signer::RequestId);
   void onVerifyOfflineTxRequest(const std::string &data, bs::signer::RequestId);

   void requestPasswordForTx(signer::PasswordDialogType reqType, bs::sync::PasswordDialogData *dialogData
      , bs::wallet::TXInfo *txInfo, bs::hd::WalletInfo *walletInfo);
   void requestPasswordForSettlementTx(signer::PasswordDialogType reqType, bs::sync::PasswordDialogData *dialogData
      , bs::wallet::TXInfo *txInfo, bs::hd::WalletInfo *walletInfo);

   void shutdown();

   bs::signer::QmlCallbackBase *createQmlPasswordCallback();

protected:
   void requestPasswordForDialogType(bs::signer::ui::PasswordInputDialogType dialogType
      , bs::sync::PasswordDialogData* dialogData, bs::hd::WalletInfo* walletInfo);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<DataConnection>     connection_;
   std::shared_ptr<QmlFactory>         qmlFactory_;
   SignerAdapter                       * parent_;

   bs::signer::RequestId seq_ = 1;
   std::map<bs::signer::RequestId, std::function<void(bs::error::ErrorCode errorCode, const BinaryData &)>> cbSignReqs_;
   std::map<bs::signer::RequestId, std::function<void(std::vector<bs::sync::WalletInfo>)>>  cbWalletInfo_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::HDWalletData)>>  cbHDWalletData_;
   std::map<bs::signer::RequestId, std::function<void(bs::sync::WalletData)>>    cbWalletData_;
   std::map<bs::signer::RequestId, std::function<void(const bs::sync::WatchingOnlyWallet &)>>   cbWO_;
   std::map<bs::signer::RequestId, std::function<void(const BinaryData &)>>      cbExportWO_;
   std::map<bs::signer::RequestId
      , std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)>>   cbDecryptNode_;
   std::map<bs::signer::RequestId, BasicCb> cbChangePwReqs_;
   std::map<bs::signer::RequestId, BasicCb> cbCreateHDWalletReqs_;
   std::map<bs::signer::RequestId, std::function<void(bool success, const std::string& errorMsg)>> cbDeleteHDWalletReqs_;
   std::map<bs::signer::RequestId, BasicCb> cbAutoSignReqs_;
   std::map<bs::signer::RequestId, BasicCb> cbChangeControlPwReqs_;
   std::map<bs::signer::RequestId, BasicCb> cbVerifyOfflineTxRequestReqs_;

   std::shared_ptr<QmlBridge>  qmlBridge_;

   std::queue<std::string> decryptWalletRequestsQueue_;

   bool isWalletsSynchronized_{false};

};


#endif // SIGNER_INTERFACE_LISTENER_H
