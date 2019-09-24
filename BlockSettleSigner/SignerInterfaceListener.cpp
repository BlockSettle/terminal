#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
#include <QObject>
#include <QVariant>
#include <QStandardPaths>
#include <QApplication>
#include "CelerClientConnection.h"
#include "DataConnection.h"
#include "DataConnectionListener.h"
#include "HeadlessApp.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include "SignerInterfaceListener.h"
#include "SignerAdapterContainer.h"
#include <memory>

using namespace bs::sync;
using namespace bs::signer;

SignerInterfaceListener::SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<QmlBridge> &qmlBridge
   , const std::shared_ptr<ZmqBIP15XDataConnection> &conn
   , SignerAdapter *parent)
   : logger_(logger)
   , connection_(conn)
   , parent_(parent)
   , qmlBridge_(qmlBridge)
{
}

void SignerInterfaceListener::OnDataReceived(const std::string &data)
{
   QMetaObject::invokeMethod(this, [this, data] {
      processData(data);
   });
}

void SignerInterfaceListener::OnConnected()
{
   logger_->info("[SignerInterfaceListener] connected");
   send(signer::HeadlessReadyType, "");
}

void SignerInterfaceListener::OnDisconnected()
{
   // Signer interface should not be used without signer, we could quit safely
   logger_->info("[SignerInterfaceListener] disconnected, shutdown");
   shutdown();
}

void SignerInterfaceListener::OnError(DataConnectionError errorCode)
{
   logger_->info("[SignerInterfaceListener] error {}, shutdown", errorCode);
   shutdown();
}

bs::signer::RequestId SignerInterfaceListener::send(signer::PacketType pt, const std::string &data)
{
   logger_->debug("send packet {}", signer::PacketType_Name(pt));

   const auto reqId = seq_++;
   signer::Packet packet;
   packet.set_id(reqId);
   packet.set_type(pt);
   packet.set_data(data);
   if (!connection_->send(packet.SerializeAsString())) {
      return 0;
   }
   return reqId;
}

void SignerInterfaceListener::processData(const std::string &data)
{
   signer::Packet packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse packet", __func__);
      return;
   }
/*   if (packet.data().empty()) {
      logger_->error("[SignerInterfaceListener::{}] error processing type {} id {}"
         , __func__, packet.type(), packet.id());
      return;
   }*/

   switch (packet.type()) {
   case signer::HeadlessReadyType:
      onReady(packet.data());
      break;
   case signer::PeerConnectedType:
      onPeerConnected(packet.data(), true);
      break;
   case signer::PeerDisconnectedType:
      onPeerConnected(packet.data(), false);
      break;
   case signer::DecryptWalletRequestType:
      onDecryptWalletRequested(packet.data());
      break;
   case signer::UpdateDialogDataType:
      onUpdateDialogData(packet.data(), packet.id());
      break;
   case signer::CancelTxSignType:
      onCancelTx(packet.data(), packet.id());
      break;
   case signer::TxSignedType:
   case signer::SignOfflineTxRequestType:
      onTxSigned(packet.data(), packet.id());
      break;
   case signer::XbtSpentType:
      onXbtSpent(packet.data());
      break;
   case signer::AutoSignActType:
      onAutoSignActivated(packet.data(), packet.id());
      break;
   case signer::SyncWalletInfoType:
      onSyncWalletInfo(packet.data(), packet.id());
      break;
   case signer::SyncHDWalletType:
      onSyncHDWallet(packet.data(), packet.id());
      break;
   case signer::SyncWalletType:
      onSyncWallet(packet.data(), packet.id());
      break;
   case signer::ImportWoWalletType:
      onCreateWO(packet.data(), packet.id());
      break;
   case signer::ExportWoWalletType:
      onExportWO(packet.data(), packet.id());
      break;
   case signer::CreateWOType:
   case signer::GetDecryptedNodeType:
      onDecryptedKey(packet.data(), packet.id());
      break;
   case signer::ReloadWalletsType:
      onReloadWallets(packet.id());
      break;
   case signer::ExecCustomDialogRequestType:
      onExecCustomDialog(packet.data(), packet.id());
      break;
   case signer::ChangePasswordType:
      onChangePassword(packet.data(), packet.id());
      break;
   case signer::CreateHDWalletType:
      onCreateHDWallet(packet.data(), packet.id());
      break;
   case signer::DeleteHDWalletType:
      onDeleteHDWallet(packet.data(), packet.id());
      break;
   case signer::WalletsListUpdatedType:
      parent_->walletsListUpdated();
      break;
   case signer::UpdateWalletType:
      onUpdateWallet(packet.data(), packet.id());
      break;
   case signer::UpdateStatusType:
      onUpdateStatus(packet.data());
      break;
   case signer::TerminalEventType:
      onTerminalEvent(packet.data());
      break;
   default:
      logger_->warn("[SignerInterfaceListener::{}] unknown response type {}", __func__, packet.type());
      break;
   }
}

void SignerInterfaceListener::onReady(const std::string &data)
{
   logger_->info("received ready signal");
   emit parent_->ready();
}

void SignerInterfaceListener::onPeerConnected(const std::string &data, bool connected)
{
   signer::PeerEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto ip = QString::fromStdString(evt.ip_address());
   if (connected) {
      emit parent_->peerConnected(ip);;
   }
   else {
      emit parent_->peerDisconnected(ip);;
   }
}

void SignerInterfaceListener::onDecryptWalletRequested(const std::string &data)
{
   signer::DecryptWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   headless::SignTxRequest txRequest = request.signtxrequest();
   bs::sync::PasswordDialogData *dialogData = new bs::sync::PasswordDialogData(request.passworddialogdata());
   QQmlEngine::setObjectOwnership(dialogData, QQmlEngine::JavaScriptOwnership);

   bs::wallet::TXInfo *txInfo = new bs::wallet::TXInfo(txRequest, parent_->walletsMgr_, logger_);
   const auto settlementId = BinaryData::CreateFromHex(
      dialogData->value(PasswordDialogData::SettlementId).toString().toStdString());
   if (!settlementId.isNull()) {
      txInfo->setTxId(QString::fromStdString(settlementId.toBinStr()));
   }
   QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

   // wallet id may be stored either in tx or in dialog data
   QString rootId = dialogData->value(PasswordDialogData::WalletId).toString();
   if (rootId.isEmpty()) {
      rootId = txInfo->walletId();
   }
   bs::hd::WalletInfo *walletInfo = qmlFactory_->createWalletInfo(rootId);
   if (rootId.isEmpty()) {
      signer::DecryptWalletEvent decryptEvent;
      decryptEvent.set_wallet_id(rootId.toStdString());
      decryptEvent.set_errorcode(static_cast<uint32_t>(bs::error::ErrorCode::WalletNotFound));
      send(signer::PasswordReceivedType, decryptEvent.SerializeAsString());
   }

   QString notifyMsg = tr("Enter password for %1").arg(walletInfo->name());

   if (dialogData->hasDialogType()) {
      // Dialog type might be set explicitly set by caller
      // Currently it using for CreateHDLeaf request which can display
      // "RequestPasswordForToken" dialog and "RequestPasswordForAuthLeaf" depends of leaf
      qmlBridge_->invokeQmlMethod(ui::createPasswordDialogForType, createQmlPasswordCallback()
         , QVariant::fromValue(dialogData), QVariant::fromValue(walletInfo));
   }
   else {
      switch (request.dialogtype()) {
      case signer::SignTx:
      case signer::SignPartialTx:
         dialogData->setValue(PasswordDialogData::Title, tr("Sign Transaction"));
         requestPasswordForTx(request.dialogtype(), dialogData, txInfo, walletInfo);
         break;
      case signer::SignSettlementTx:
      case signer::SignSettlementPartialTx:
         requestPasswordForSettlementTx(request.dialogtype(), dialogData, txInfo, walletInfo);
         break;
      case signer::CreateAuthLeaf:
         requestPasswordForDialogType(ui::PasswordInputDialogType::RequestPasswordForAuthLeaf, dialogData, walletInfo);
         break;
      case signer::CreateHDLeaf:
         requestPasswordForDialogType(ui::PasswordInputDialogType::RequestPasswordForToken, dialogData, walletInfo);
         break;
      case signer::CreateSettlementLeaf:
         dialogData->setValue(PasswordDialogData::Title, tr("Create Authentication Address"));
         requestPasswordForDialogType(ui::PasswordInputDialogType::RequestPasswordForSettlementLeaf, dialogData, walletInfo);
         break;
      case signer::RevokeAuthAddress:
         dialogData->setValue(PasswordDialogData::Title, tr("Revoke Authentication Address"));
         requestPasswordForDialogType(ui::PasswordInputDialogType::RequestPasswordForRevokeAuthAddress, dialogData, walletInfo);
         break;
      case signer::PromoteHDWallet:
         requestPasswordForDialogType(ui::PasswordInputDialogType::RequestPasswordForPromoteHDWallet, dialogData, walletInfo);
         break;
      default:
         break;
      }
   }

   emit qmlFactory_->showTrayNotify(dialogData->value(PasswordDialogData::Title).toString(), notifyMsg);
}

void SignerInterfaceListener::onTxSigned(const std::string &data, bs::signer::RequestId reqId)
{
   bs::error::ErrorCode result = bs::error::ErrorCode::NoError;
   BinaryData tx;
   signer::SignTxEvent evt;

   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      result = bs::error::ErrorCode::TxInvalidRequest;
   }
   else {
      result = static_cast<bs::error::ErrorCode>(evt.errorcode());
      if (result == bs::error::ErrorCode::NoError) {
         tx = evt.signedtx();
         emit parent_->txSigned(tx);
      }
      else {
         logger_->error("[SignerInterfaceListener::{}] error on signing tx: {}", __func__, evt.errorcode());
      }
   }

   if (reqId) {
      const auto &itCb = cbSignReqs_.find(reqId);
      if (itCb != cbSignReqs_.end()) {
         itCb->second(result, tx);
         cbSignReqs_.erase(itCb);
      } else {
         logger_->debug("[SignerInterfaceListener::{}] failed to find callback for id {}"
            , __func__, reqId);
      }
   }
}

void SignerInterfaceListener::onUpdateDialogData(const std::string &data, RequestId)
{
   headless::UpdateDialogDataRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   bs::sync::PasswordDialogData *dialogData = new bs::sync::PasswordDialogData(request.passworddialogdata());
   QQmlEngine::setObjectOwnership(dialogData, QQmlEngine::JavaScriptOwnership);

   qmlBridge_->invokeQmlMethod("updateDialogData", nullptr
      , QVariant::fromValue(dialogData));
}

void SignerInterfaceListener::onCancelTx(const std::string &data, bs::signer::RequestId)
{
   headless::CancelSignTx evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   QMetaObject::invokeMethod(parent_, [this, evt] {
      emit parent_->cancelTxSign(evt.tx_id());
   });
}

void SignerInterfaceListener::onXbtSpent(const std::string &data)
{
   signer::XbtSpentEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   emit parent_->xbtSpent(evt.value(), evt.auto_sign());
}

void SignerInterfaceListener::onAutoSignActivated(const std::string &data, bs::signer::RequestId reqId)
{
   signer::AutoSignActResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   const auto &itCb = cbAutoSignReqs_.find(reqId);
   if (itCb == cbAutoSignReqs_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }

   bs::error::ErrorCode result = static_cast<bs::error::ErrorCode>(response.errorcode());
   if (result == bs::error::ErrorCode::NoError) {
      if (response.autosignactive()) {
         emit parent_->autoSignActivated(response.rootwalletid());
      }
      else {
         emit parent_->autoSignDeactivated(response.rootwalletid());
      }
   }

   itCb->second(result);
   cbAutoSignReqs_.erase(itCb);
}

void SignerInterfaceListener::onSyncWalletInfo(const std::string &data, bs::signer::RequestId reqId)
{
   headless::SyncWalletInfoResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbWalletInfo_.find(reqId);
   if (itCb == cbWalletInfo_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   std::vector<bs::sync::WalletInfo> result = bs::sync::WalletInfo::fromPbMessage(response);

   itCb->second(result);
   cbWalletInfo_.erase(itCb);
}

void SignerInterfaceListener::onSyncHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SyncHDWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbHDWalletData_.find(reqId);
   if (itCb == cbHDWalletData_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   bs::sync::HDWalletData result;
   for (int i = 0; i < response.groups_size(); ++i) {
      const auto group = response.groups(i);
      std::vector<bs::sync::HDWalletData::Leaf> leaves;
      for (int j = 0; j < group.leaves_size(); ++j) {
         const auto leaf = group.leaves(j);
         leaves.push_back({ leaf.id(), bs::hd::Path::fromString(leaf.path())
            , false, leaf.extra_data() });
      }
      result.groups.push_back({ static_cast<bs::hd::CoinType>(group.type()), leaves });
   }
   itCb->second(result);
   cbHDWalletData_.erase(itCb);
}

void SignerInterfaceListener::onSyncWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SyncWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbWalletData_.find(reqId);
   if (itCb == cbWalletData_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   bs::sync::WalletData result;

   result.highestExtIndex = response.highest_ext_index();
   result.highestIntIndex = response.highest_int_index();

   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto addr = response.addresses(i);
      result.addresses.push_back({ addr.index(), addr.address(), {} });
   }
   itCb->second(result);
   cbWalletData_.erase(itCb);
}

void SignerInterfaceListener::onCreateWO(const std::string &data, bs::signer::RequestId reqId)
{
   signer::CreateWatchingOnlyResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbWO_.find(reqId);
   if (itCb == cbWO_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   bs::sync::WatchingOnlyWallet result;
   result.id = response.wallet_id();
   result.name = response.name();
   result.description = response.description();
   for (int i = 0; i < response.groups_size(); ++i) {
      const auto group = response.groups(i);
      std::vector<bs::sync::WatchingOnlyWallet::Leaf> leaves;
      for (int j = 0; j < group.leaves_size(); ++j) {
         const auto leaf = group.leaves(j);
         std::vector<bs::sync::WatchingOnlyWallet::Address> addresses;
         for (int k = 0; k < leaf.addresses_size(); ++k) {
            const auto addr = leaf.addresses(k);
            addresses.push_back({ addr.index()
               , static_cast<AddressEntryType>(addr.aet()) });
         }
         leaves.push_back({ leaf.id(), bs::hd::Path::fromString(leaf.path()), leaf.public_key()
            , leaf.chain_code(), addresses });
      }
      result.groups.push_back({ static_cast<bs::hd::CoinType>(group.type()), leaves });
   }
   result.netType = parent_->netType();
   itCb->second(result);
   cbWO_.erase(itCb);
}

void SignerInterfaceListener::onExportWO(const std::string &data, RequestId reqId)
{
   signer::ExportWoWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbExportWO_.find(reqId);
   if (itCb == cbExportWO_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "failed to find callback for id {}", reqId);
      return;
   }

   itCb->second(BinaryData(response.content()));
   cbExportWO_.erase(itCb);
}

void SignerInterfaceListener::onDecryptedKey(const std::string &data, bs::signer::RequestId reqId)
{
   signer::DecryptedNodeResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbDecryptNode_.find(reqId);
   if (itCb == cbDecryptNode_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   itCb->second(response.private_key(), response.chain_code());
   cbDecryptNode_.erase(itCb);
}

void SignerInterfaceListener::onReloadWallets(bs::signer::RequestId reqId)
{
   const auto &itCb = cbReloadWallets_.find(reqId);
   if (itCb == cbReloadWallets_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   itCb->second();
   cbReloadWallets_.erase(itCb);
}

void SignerInterfaceListener::onExecCustomDialog(const std::string &data, bs::signer::RequestId)
{
   signer::CustomDialogRequest evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   // deserialize variant data
   QByteArray ba = QByteArray::fromStdString(evt.variantdata());
   QDataStream stream(&ba, QIODevice::ReadOnly);
   QVariantMap variantData;
   stream >> variantData;

   emit parent_->customDialogRequest(QString::fromStdString(evt.dialogname()), variantData);
}

void SignerInterfaceListener::onChangePassword(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ChangePasswordResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbChangePwReqs_.find(reqId);
   if (itCb == cbChangePwReqs_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   itCb->second(response.success());
   cbChangePwReqs_.erase(itCb);
}

void SignerInterfaceListener::onCreateHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::CreateHDWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbCreateHDWalletReqs_.find(reqId);
   if (itCb == cbCreateHDWalletReqs_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   itCb->second(static_cast<bs::error::ErrorCode>(response.errorcode()));
   cbCreateHDWalletReqs_.erase(itCb);
}

void SignerInterfaceListener::onDeleteHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   headless::DeleteHDWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbDeleteHDWalletReqs_.find(reqId);
   if (itCb == cbDeleteHDWalletReqs_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   itCb->second(response.success(), response.error());
   cbDeleteHDWalletReqs_.erase(itCb);
}

void SignerInterfaceListener::onUpdateWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::UpdateWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   parent_->updateWallet(request.wallet_id());
}

void SignerInterfaceListener::onUpdateStatus(const std::string &data)
{
   signer::UpdateStatus evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   emit parent_->headlessBindUpdated(bs::signer::BindStatus(evt.signer_bind_status()));
   emit parent_->signerPubKeyUpdated(evt.signer_pub_key());
}

void SignerInterfaceListener::onTerminalEvent(const std::string &data)
{
   logger_->debug("[{}]", __func__);
   signer::TerminalEvent evt;
   if (!evt.ParseFromString(data)) {
      SPDLOG_LOGGER_ERROR(logger_, "failed to parse");
      return;
   }

   logger_->debug("[{}] cc info received: {}", __func__, evt.cc_info_received());
   if (!evt.peer_address().empty() && !evt.handshake_ok()) {
      emit parent_->terminalHandshakeFailed(evt.peer_address());
   }
   else {
      emit parent_->ccInfoReceived(evt.cc_info_received());
   }
}

void SignerInterfaceListener::requestPasswordForTx(signer::PasswordDialogType reqType
   , bs::sync::PasswordDialogData *dialogData, bs::wallet::TXInfo *txInfo, bs::hd::WalletInfo *walletInfo)
{
   bool partial = (reqType == signer::SignPartialTx);
   QString prompt = (partial ? tr("Outgoing Partial Transaction") : tr("Outgoing Transaction"));

   qmlBridge_->invokeQmlMethod("createTxSignDialog", createQmlPasswordCallback()
      , QVariant::fromValue(txInfo)
      , QVariant::fromValue(dialogData)
      , QVariant::fromValue(walletInfo));
}

void SignerInterfaceListener::requestPasswordForSettlementTx(signer::PasswordDialogType reqType
   , bs::sync::PasswordDialogData *dialogData, bs::wallet::TXInfo *txInfo, bs::hd::WalletInfo *walletInfo)
{
   bool partial = (reqType == signer::SignSettlementPartialTx);
   QString prompt = (partial ? tr("Outgoing Partial Transaction") : tr("Outgoing Transaction"));

   qmlBridge_->invokeQmlMethod("createTxSignSettlementDialog", createQmlPasswordCallback()
      , QVariant::fromValue(txInfo)
      , QVariant::fromValue(dialogData)
      , QVariant::fromValue(walletInfo));
}

void SignerInterfaceListener::shutdown()
{
   QMetaObject::invokeMethod(qApp, [] {
      QApplication::quit();
   });
}

void SignerInterfaceListener::closeConnection()
{
   connection_->closeConnection();
}

QmlCallbackBase *SignerInterfaceListener::createQmlPasswordCallback()
{
   return new bs::signer::QmlCallback<int, QString, bs::wallet::QPasswordData *>
         ([this](int result, const QString &walletId, bs::wallet::QPasswordData *passwordData){
      signer::DecryptWalletEvent decryptEvent;
      decryptEvent.set_wallet_id(walletId.toStdString());
      if (passwordData) {
         decryptEvent.set_password(passwordData->binaryPassword().toBinStr());
      }
      decryptEvent.set_errorcode(static_cast<uint32_t>(result));
      send(signer::PasswordReceivedType, decryptEvent.SerializeAsString());
   });
}

void SignerInterfaceListener::requestPasswordForDialogType(ui::PasswordInputDialogType dialogType
   , bs::sync::PasswordDialogData* dialogData, bs::hd::WalletInfo* walletInfo)
{
   dialogData->setValue(PasswordDialogData::DialogType
      , bs::signer::ui::getPasswordInputDialogName(dialogType));
   qmlBridge_->invokeQmlMethod(bs::signer::ui::createPasswordDialogForType, createQmlPasswordCallback()
      , QVariant::fromValue(dialogData), QVariant::fromValue(walletInfo));
}

void SignerInterfaceListener::setQmlFactory(const std::shared_ptr<QmlFactory> &qmlFactory)
{
   qmlFactory_ = qmlFactory;
}
