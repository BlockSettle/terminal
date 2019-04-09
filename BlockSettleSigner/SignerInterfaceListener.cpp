#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
#include <QStandardPaths>
#include "CelerClientConnection.h"
#include "DataConnection.h"
#include "DataConnectionListener.h"
#include "HeadlessApp.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include "SignerInterfaceListener.h"
#include "SignerAdapterContainer.h"

void SignerInterfaceListener::OnDataReceived(const std::string &data)
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
   case signer::PasswordRequestType:
      onPasswordRequested(packet.data());
      break;
   case signer::TxSignedType:
   case signer::SignTxRequestType:
      onTxSigned(packet.data(), packet.id());
      break;
   case signer::XbtSpentType:
      onXbtSpent(packet.data());
      break;
   case signer::AutoSignActType:
      onAutoSignActivate(packet.data());
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
   case signer::CreateWOType:
      onCreateWO(packet.data(), packet.id());
      break;
   case signer::GetDecryptedNodeType:
      onDecryptedKey(packet.data(), packet.id());
      break;
   case signer::ReloadWalletsType:
      onReloadWallets(packet.id());
      break;
   case signer::ExecCustomDialogRequestType:
      onExecCustomDialog(packet.data(), packet.id());
      break;
   case signer::ChangePasswordRequestType:
      onChangePassword(packet.data(), packet.id());
      break;
   default:
      logger_->warn("[SignerInterfaceListener::{}] unknown response type {}", __func__, packet.type());
      break;
   }
}

void SignerInterfaceListener::OnConnected() {
   logger_->debug("[SignerInterfaceListener] connected");
   send(signer::HeadlessReadyType, "");
}

void SignerInterfaceListener::OnDisconnected() {
   logger_->debug("[SignerInterfaceListener] disconnected");
}

void SignerInterfaceListener::OnError(DataConnectionError errorCode) {
   logger_->debug("[SignerInterfaceListener] error {}", errorCode);
}

SignContainer::RequestId SignerInterfaceListener::send(signer::PacketType pt, const std::string &data) {
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

void SignerInterfaceListener::onReady(const std::string &data)
{
   signer::ReadyEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   if (evt.ready()) {
      QMetaObject::invokeMethod(parent_, [this] { emit parent_->ready(); });
   }
   else {
      logger_->info("[SignerInterfaceListener::{}] received 'non-ready' event {} of {}"
         , __func__, evt.cur_wallet(), evt.total_wallets());
   }
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
      QMetaObject::invokeMethod(parent_, [this, ip] { emit parent_->peerConnected(ip); });
   }
   else {
      QMetaObject::invokeMethod(parent_, [this, ip] { emit parent_->peerDisconnected(ip); });
   }
}

void SignerInterfaceListener::onPasswordRequested(const std::string &data)
{
   signer::PasswordEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   if (evt.auto_sign()) {
      QMetaObject::invokeMethod(parent_, [this, evt] {
         emit parent_->autoSignRequiresPwd(evt.wallet_id()); });
      return;
   }
   bs::core::wallet::TXSignRequest txReq;
   txReq.walletId = evt.wallet_id();
   for (int i = 0; i < evt.inputs_size(); ++i) {
      UTXO utxo;
      utxo.unserialize(evt.inputs(i));
      txReq.inputs.emplace_back(std::move(utxo));
   }
   for (int i = 0; i < evt.recipients_size(); ++i) {
      const BinaryData bd(evt.recipients(i));
      txReq.recipients.push_back(ScriptRecipient::deserialize(bd));
   }
   txReq.fee = evt.fee();
   txReq.RBF = evt.rbf();
   if (evt.has_change()) {
      txReq.change.address = evt.change().address();
      txReq.change.index = evt.change().index();
      txReq.change.value = evt.change().value();
   }
   QMetaObject::invokeMethod(parent_, [this, txReq, evt] {
      emit parent_->requestPassword(txReq, QString::fromStdString(evt.prompt()));
   });
}

void SignerInterfaceListener::onTxSigned(const std::string &data, SignContainer::RequestId reqId)
{
   signer::TxSignEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   if (!evt.tx_hash().empty()) {
      QMetaObject::invokeMethod(parent_, [this, evt] {
         emit parent_->cancelTxSign(evt.tx_hash());
      });
      return;
   }
   const BinaryData tx(evt.tx());
   if (tx.isNull()) {
      logger_->error("[SignerInterfaceListener::{}] empty TX data", __func__);
      return;
   }
   if (reqId) {
      const auto &itCb = cbSignReqs_.find(reqId);
      if (itCb != cbSignReqs_.end()) {
         itCb->second(tx);
         cbSignReqs_.erase(itCb);
      }
      else {
         logger_->debug("[SignerInterfaceListener::{}] failed to find callback for id {}"
            , __func__, reqId);
      }
   }
   QMetaObject::invokeMethod(parent_, [this, tx] { emit parent_->txSigned(tx); });
}

void SignerInterfaceListener::onXbtSpent(const std::string &data)
{
   signer::XbtSpentEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   QMetaObject::invokeMethod(parent_, [this, evt] {
      emit parent_->xbtSpent(evt.value(), evt.auto_sign());
   });
}

void SignerInterfaceListener::onAutoSignActivate(const std::string &data)
{
   signer::AutoSignActEvent evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   if (evt.activated()) {
      QMetaObject::invokeMethod(parent_, [this, evt] { emit parent_->autoSignActivated(evt.wallet_id()); });
   }
   else {
      QMetaObject::invokeMethod(parent_, [this, evt] { emit parent_->autoSignDeactivated(evt.wallet_id()); });
   }
}

void SignerInterfaceListener::onSyncWalletInfo(const std::string &data, SignContainer::RequestId reqId)
{
   signer::SyncWalletInfoResponse response;
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
   std::vector<bs::sync::WalletInfo> result;
   for (int i = 0; i < response.wallets_size(); ++i) {
      const auto wallet = response.wallets(i);
      const auto format = (wallet.format() == signer::WalletFormatHD) ? bs::sync::WalletFormat::HD
         : bs::sync::WalletFormat::Settlement;
      result.push_back({ format, wallet.id(), wallet.name(), wallet.description()
         , parent_->netType() });
   }
   itCb->second(result);
   cbWalletInfo_.erase(itCb);
}

void SignerInterfaceListener::onSyncHDWallet(const std::string &data, SignContainer::RequestId reqId)
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
         leaves.push_back({ leaf.id(), leaf.index() });
      }
      result.groups.push_back({ static_cast<bs::hd::CoinType>(group.type()), leaves });
   }
   itCb->second(result);
   cbHDWalletData_.erase(itCb);
}

void SignerInterfaceListener::onSyncWallet(const std::string &data, SignContainer::RequestId reqId)
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
   for (int i = 0; i < response.encryption_types_size(); ++i) {
      result.encryptionTypes.push_back(static_cast<bs::wallet::EncryptionType>(
         response.encryption_types(i)));
   }
   for (int i = 0; i < response.encryption_keys_size(); ++i) {
      result.encryptionKeys.push_back(response.encryption_keys(i));
   }
   result.encryptionRank = { response.key_rank_m(), response.key_rank_n() };
   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto addr = response.addresses(i);
      result.addresses.push_back({ addr.index(), addr.address(), {} });
   }
   itCb->second(result);
   cbWalletData_.erase(itCb);
}

void SignerInterfaceListener::onCreateWO(const std::string &data, SignContainer::RequestId reqId)
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
         leaves.push_back({ leaf.id(), leaf.index(), leaf.public_key()
            , leaf.chain_code(), addresses });
      }
      result.groups.push_back({ static_cast<bs::hd::CoinType>(group.type()), leaves });
   }
   result.netType = parent_->netType();
   itCb->second(result);
   cbWO_.erase(itCb);
}

void SignerInterfaceListener::onDecryptedKey(const std::string &data, SignContainer::RequestId reqId)
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

void SignerInterfaceListener::onReloadWallets(SignContainer::RequestId reqId)
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

void SignerInterfaceListener::onExecCustomDialog(const std::string &data, SignContainer::RequestId)
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


   QMetaObject::invokeMethod(parent_, [this, evt, variantData] {
      emit parent_->customDialogRequest(QString::fromStdString(evt.dialogname()), variantData);
   });
}

void SignerInterfaceListener::onChangePassword(const std::string &data, SignContainer::RequestId reqId)
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
