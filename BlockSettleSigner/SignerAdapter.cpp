#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include "CelerClientConnection.h"
#include "DataConnection.h"
#include "DataConnectionListener.h"
#include "HeadlessApp.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqContext.h"
#include "ZmqDataConnection.h"

#include "bs_signer.pb.h"

using namespace Blocksettle::Communication;

class SignerInterfaceListener : public DataConnectionListener
{
public:
   SignerInterfaceListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<DataConnection> &conn, SignerAdapter *parent)
      : logger_(logger), connection_(conn), parent_(parent) {}

   void OnDataReceived(const std::string &) override;

   void OnConnected() override {
      logger_->debug("[SignerInterfaceListener] connected");
      send(signer::HeadlessReadyType, "");
   }

   void OnDisconnected() override {
      logger_->debug("[SignerInterfaceListener] disconnected");
   }

   void OnError(DataConnectionError errorCode) override {
      logger_->debug("[SignerInterfaceListener] error {}", errorCode);
   }

   SignContainer::RequestId send(signer::PacketType pt, const std::string &data) {
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

private:
   void onReady(const std::string &data);
   void onPeerConnected(const std::string &data, bool connected);
   void onPasswordRequested(const std::string &data);
   void onTxSigned(const std::string &data, SignContainer::RequestId);
   void onXbtSpent(const std::string &data);
   void onAutoSignActType(const std::string &data);
   void onSyncWalletInfo(const std::string &data, SignContainer::RequestId);
   void onSyncHDWallet(const std::string &data, SignContainer::RequestId);
   void onSyncWallet(const std::string &data, SignContainer::RequestId);
   void onCreateWO(const std::string &data, SignContainer::RequestId);
   void onDecryptedKey(const std::string &data, SignContainer::RequestId);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<DataConnection>  connection_;
   SignerAdapter  *  parent_;
   SignContainer::RequestId   seq_ = 1;
   std::map<SignContainer::RequestId, std::function<void(const BinaryData &)>>      cbSignReqs_;
   std::map<SignContainer::RequestId, std::function<void(std::vector<bs::sync::WalletInfo>)>>  cbWalletInfo_;
   std::map<SignContainer::RequestId, std::function<void(bs::sync::HDWalletData)>>  cbHDWalletData_;
   std::map<SignContainer::RequestId, std::function<void(bs::sync::WalletData)>>    cbWalletData_;
   std::map<SignContainer::RequestId, std::function<void(const bs::sync::WatchingOnlyWallet &)>>   cbWO_;
   std::map<SignContainer::RequestId
      , std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)>>   cbDecryptNode_;
};

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
      onAutoSignActType(packet.data());
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
   default:
      logger_->warn("[SignerInterfaceListener::{}] unknown response type {}", __func__, packet.type());
      break;
   }
}

static std::string toHex(const std::string &binData)
{
   return BinaryData(binData).toHexStr();
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

void SignerInterfaceListener::onAutoSignActType(const std::string &data)
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


class SignAdapterContainer : public SignContainer
{
public:
   SignAdapterContainer(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<SignerInterfaceListener> &lsn)
      : SignContainer(logger, OpMode::LocalInproc), listener_(lsn)
   {}
   ~SignAdapterContainer() noexcept = default;

   RequestId signTXRequest(const bs::core::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
   , bool keepDuplicatedRecipients = false) override;
   RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) override { return 0; }
   RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, bool autoSign = false, const PasswordType& password = {}) override {
      return 0;
   }
   RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override { return 0; }

   void SendPassword(const std::string &walletId, const PasswordType &password,
      bool cancelledByUser) override {}
   RequestId CancelSignTx(const BinaryData &txId) override { return 0; }

   RequestId SetUserId(const BinaryData &) override { return 0; }
   RequestId createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}) override { return 0; }
   RequestId createHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::core::wallet::Seed &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 }) override {
      return 0;
   }
   RequestId DeleteHDRoot(const std::string &rootWalletId) override { return 0; }
   RequestId DeleteHDLeaf(const std::string &leafWalletId) override { return 0; }
   RequestId getDecryptedRootKey(const std::string &walletId, const SecureBinaryData &password = {}) override { return 0; }
   RequestId GetInfo(const std::string &rootWalletId) override { return 0; }
   void setLimits(const std::string &walletId, const SecureBinaryData &password, bool autoSign) override {}
   RequestId changePassword(const std::string &walletId, const std::vector<bs::wallet::PasswordData> &newPass
      , bs::wallet::KeyRank, const SecureBinaryData &oldPass, bool addNew, bool removeOld, bool dryRun) override { return 0; }

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;
   void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) override {}
   void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) override {}
   void syncNewAddress(const std::string &walletId, const std::string &index, AddressEntryType
      , const std::function<void(const bs::Address &)> &) override {}
   void syncNewAddresses(const std::string &walletId, const std::vector<std::pair<std::string, AddressEntryType>> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &, bool persistent = true) override {}

private:
   std::shared_ptr<SignerInterfaceListener>  listener_;
};

SignContainer::RequestId SignAdapterContainer::signTXRequest(const bs::core::wallet::TXSignRequest &txReq
   , bool autoSign, TXSignMode mode, const PasswordType& password, bool keepDuplicatedRecipients)
{
   signer::SignTxRequest request;
   request.set_password(password.toBinStr());
   auto evt = request.mutable_tx_request();

   evt->set_wallet_id(txReq.walletId);
   for (const auto &input : txReq.inputs) {
      evt->add_inputs(input.serialize().toBinStr());
   }
   for (const auto &recip : txReq.recipients) {
      evt->add_recipients(recip->getSerializedScript().toBinStr());
   }
   evt->set_fee(txReq.fee);
   evt->set_rbf(txReq.RBF);
   if (txReq.change.value) {
      auto change = evt->mutable_change();
      change->set_address(txReq.change.address.display<std::string>());
      change->set_index(txReq.change.index);
      change->set_value(txReq.change.value);
   }

   return listener_->send(signer::SignTxRequestType, request.SerializeAsString());
}

void SignAdapterContainer::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   const auto reqId = listener_->send(signer::SyncWalletInfoType, "");
   listener_->setWalleteInfoCb(reqId, cb);
}

void SignAdapterContainer::syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &cb)
{
   signer::SyncWalletRequest request;
   request.set_wallet_id(id);
   const auto reqId = listener_->send(signer::SyncHDWalletType, request.SerializeAsString());
   listener_->setHDWalletDataCb(reqId, cb);
}

void SignAdapterContainer::syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &cb)
{
   signer::SyncWalletRequest request;
   request.set_wallet_id(id);
   const auto reqId = listener_->send(signer::SyncWalletType, request.SerializeAsString());
   listener_->setWalletDataCb(reqId, cb);
}

SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger, NetworkType netType)
   : QObject(nullptr), logger_(logger), netType_(netType)
{
   const auto zmqContext = std::make_shared<ZmqContext>(logger);
   auto adapterConn = std::make_shared<CelerClientConnection<ZmqDataConnection>>(logger);
   adapterConn->SetContext(zmqContext);
   listener_ = std::make_shared<SignerInterfaceListener>(logger, adapterConn, this);
   if (!adapterConn->openConnection("127.0.0.1", "23457", listener_.get())) {
      throw std::runtime_error("adapter connection failed");
   }

   signContainer_ = std::make_shared<SignAdapterContainer>(logger_, listener_);
}

SignerAdapter::~SignerAdapter()
{
   listener_->send(signer::RequestCloseType, "");
}

std::shared_ptr<bs::sync::WalletsManager> SignerAdapter::getWalletsManager()
{
   if (!walletsMgr_) {
      walletsMgr_ = std::make_shared<bs::sync::WalletsManager>(logger_, nullptr, nullptr);
      signContainer_->Start();
      walletsMgr_->setSignContainer(signContainer_);
   }
   return walletsMgr_;
}

void SignerAdapter::signTxRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password, const std::function<void(const BinaryData &)> &cb)
{
   const auto reqId = signContainer_->signTXRequest(txReq, false, SignContainer::TXSignMode::Full, password, true);
   listener_->setTxSignCb(reqId, cb);
}

void SignerAdapter::createWatchingOnlyWallet(const QString &walletId, const SecureBinaryData &password
   , const std::function<void(const bs::sync::WatchingOnlyWallet &)> &cb)
{
   signer::DecryptWalletRequest request;
   request.set_wallet_id(walletId.toStdString());
   request.set_password(password.toBinStr());
   const auto reqId = listener_->send(signer::CreateWOType, request.SerializeAsString());
   listener_->setWatchOnlyCb(reqId, cb);
}

void SignerAdapter::getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
   , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb)
{
   signer::DecryptWalletRequest request;
   request.set_wallet_id(walletId);
   request.set_password(password.toBinStr());
   const auto reqId = listener_->send(signer::GetDecryptedNodeType, request.SerializeAsString());
   listener_->setDecryptNodeCb(reqId, cb);
}

void SignerAdapter::reloadWallets(const QString &walletsDir, const std::function<void()> &cb)
{
//   app_->reloadWallets(walletsDir.toStdString(), cb);  //TODO later - maybe not needed at all
}

void SignerAdapter::setOnline(bool value)
{
//   app_->setOnline(value);  //TODO later - maybe not needed at all
}

void SignerAdapter::reconnect(const QString &address, const QString &port)
{
//   app_->reconnect(address.toStdString(), port.toStdString());  //TODO later
}

void SignerAdapter::setLimits(SignContainer::Limits limits)
{
   signer::SetLimitsRequest request;
   request.set_auto_sign_satoshis(limits.autoSignSpendXBT);
   request.set_manual_satoshis(limits.manualSpendXBT);
   request.set_auto_sign_time(limits.autoSignTimeS);
   request.set_password_keep_in_mem(limits.manualPassKeepInMemS);
   listener_->send(signer::SetLimitsType, request.SerializeAsString());
}

void SignerAdapter::passwordReceived(const std::string &walletId
   , const SecureBinaryData &password, bool cancelledByUser)
{
   signer::DecryptWalletRequest request;
   request.set_wallet_id(walletId);
   request.set_password(password.toBinStr());
   request.set_cancelled_by_user(cancelledByUser);
   listener_->send(signer::PasswordReceivedType, request.SerializeAsString());
}

void SignerAdapter::addPendingAutoSignReq(const std::string &walletId)
{
   //TODO later
}

void SignerAdapter::deactivateAutoSign()
{
   //TODO later
}
