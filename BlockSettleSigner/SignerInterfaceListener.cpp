#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
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


SignerInterfaceListener::SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ZmqBIP15XDataConnection> &conn
   , SignerAdapter *parent)
   : logger_(logger)
   , connection_(conn)
   , parent_(parent)
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
   case signer::PasswordRequestType:
      onPasswordRequested(packet.data());
      break;
   case signer::CancelTxSignType:
   case signer::TxSignedType:
   case signer::SignTxRequestType:
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
   case signer::CreateWOType:
   case signer::ImportWoWalletType:
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
   case signer::CreateHDWalletType:
      onCreateHDWallet(packet.data(), packet.id());
      break;
   case signer::DeleteHDWalletType:
      onDeleteHDWallet(packet.data(), packet.id());
      break;
   case signer::WalletsListUpdatedType:
      parent_->walletsListUpdated();
      break;
   case signer::HeadlessPubKeyRequestType:
      onHeadlessPubKey(packet.data(), packet.id());
      break;
   case signer::UpdateStatusType:
      onUpdateStatus(packet.data());
      break;
   default:
      logger_->warn("[SignerInterfaceListener::{}] unknown response type {}", __func__, packet.type());
      break;
   }
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

void SignerInterfaceListener::onTxSigned(const std::string &data, bs::signer::RequestId reqId)
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
         QMetaObject::invokeMethod(parent_, [this, response] { emit parent_->autoSignActivated(response.rootwalletid()); });
      }
      else {
         QMetaObject::invokeMethod(parent_, [this, response] { emit parent_->autoSignDeactivated(response.rootwalletid()); });
      }
   }

   itCb->second(result);
   cbAutoSignReqs_.erase(itCb);
}

void SignerInterfaceListener::onSyncWalletInfo(const std::string &data, bs::signer::RequestId reqId)
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
         , parent_->netType(), wallet.watching_only() });
   }
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
         leaves.push_back({ leaf.id(), leaf.index() });
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
         leaves.push_back({ leaf.id(), leaf.index(), leaf.public_key()
            , leaf.chain_code(), addresses });
      }
      result.groups.push_back({ static_cast<bs::hd::CoinType>(group.type()), leaves });
   }
   result.netType = parent_->netType();
   itCb->second(result);
   cbWO_.erase(itCb);
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


   QMetaObject::invokeMethod(parent_, [this, evt, variantData] {
      emit parent_->customDialogRequest(QString::fromStdString(evt.dialogname()), variantData);
   });
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
   headless::CreateHDWalletResponse response;
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
   bool success = response.error().empty();
   itCb->second(success, response.error());
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

void SignerInterfaceListener::onHeadlessPubKey(const std::string &data, bs::signer::RequestId reqId)
{
   signer::HeadlessPubKeyResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }
   const auto &itCb = cbHeadlessPubKeyReqs_.find(reqId);
   if (itCb == cbHeadlessPubKeyReqs_.end()) {
      logger_->error("[SignerInterfaceListener::{}] failed to find callback for id {}"
         , __func__, reqId);
      return;
   }
   itCb->second(response.pubkey());
   cbHeadlessPubKeyReqs_.erase(itCb);
}

void SignerInterfaceListener::onUpdateStatus(const std::string &data)
{
   signer::UpdateStatus evt;
   if (!evt.ParseFromString(data)) {
      logger_->error("[SignerInterfaceListener::{}] failed to parse", __func__);
      return;
   }

   if (evt.signer_bind_status() == signer::BindFailed) {
      QMetaObject::invokeMethod(parent_, [this] { emit parent_->headlessBindFailed(); });
   }
}

void SignerInterfaceListener::shutdown()
{
   QMetaObject::invokeMethod(qApp, [] {
      // For some reasons QApplication::quit does not work reliable.
      // Run it on main thread because otherwise it causes crash on Linux when atexit callbacks are called.
      std::exit(0);
   });
}
