#include "HeadlessContainer.h"

#include "ConnectionManager.h"
#include "Wallets/SyncSettlementWallet.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "SystemFileUtils.h"
#include "BSErrorCodeStrings.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <spdlog/spdlog.h>
#include "signer.pb.h"

namespace {

   constexpr int kKillTimeout = 5000;
   constexpr int kStartTimeout = 5000;

   // When remote signer will try to reconnect
   constexpr auto kLocalReconnectPeriod = std::chrono::seconds(10);
   constexpr auto kRemoteReconnectPeriod = std::chrono::seconds(1);

} // namespace

using namespace Blocksettle::Communication;
Q_DECLARE_METATYPE(headless::RequestPacket)
Q_DECLARE_METATYPE(std::shared_ptr<bs::sync::hd::Leaf>)

NetworkType HeadlessContainer::mapNetworkType(headless::NetworkType netType)
{
   switch (netType) {
   case headless::MainNetType:   return NetworkType::MainNet;
   case headless::TestNetType:   return NetworkType::TestNet;
   default:                      return NetworkType::Invalid;
   }
}

void HeadlessContainer::makeCreateHDWalletRequest(const std::string &name, const std::string &desc, bool primary
   , const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank
   , headless::CreateHDWalletRequest &request)
{
   if (!pwdData.empty()) {
      request.set_rankm(keyRank.first);
      request.set_rankn(keyRank.second);
   }
   for (const auto &pwd : pwdData) {
      auto reqPwd = request.add_password();
      reqPwd->set_password(pwd.password.toBinStr());
      reqPwd->set_enctype(static_cast<uint32_t>(pwd.encType));
      reqPwd->set_enckey(pwd.encKey.toBinStr());
   }
   auto wallet = request.mutable_wallet();
   wallet->set_name(name);
   wallet->set_description(desc);
   wallet->set_nettype((seed.networkType() == NetworkType::TestNet) ? headless::TestNetType : headless::MainNetType);
   if (primary) {
      wallet->set_primary(true);
   }
   if (!seed.empty()) {
      if (seed.hasPrivateKey()) {
         wallet->set_privatekey(seed.privateKey().toBinStr());
      }
      else if (!seed.seed().isNull()) {
         wallet->set_seed(seed.seed().toBinStr());
      }
   }
}

void HeadlessListener::OnDataReceived(const std::string& data)
{
   headless::RequestPacket packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[HeadlessListener] failed to parse request packet");
      return;
   }

   if (packet.id() > id_) {
      logger_->error("[HeadlessListener] reply id inconsistency: {} > {}", packet.id(), id_);
      tryEmitError(HeadlessContainer::InvalidProtocol, tr("reply id inconsistency"));
      return;
   }

   if (packet.type() == headless::DisconnectionRequestType) {
      processDisconnectNotification();
      return;
   }

   if (packet.type() == headless::AuthenticationRequestType) {
      headless::AuthenticationReply response;
      if (!response.ParseFromString(packet.data())) {
         logger_->error("[HeadlessListener] failed to parse auth reply");

         tryEmitError(HeadlessContainer::SerializationFailed, tr("failed to parse auth reply"));
         return;
      }

      if (HeadlessContainer::mapNetworkType(response.nettype()) != netType_) {
         logger_->error("[HeadlessListener] network type mismatch");
         tryEmitError(HeadlessContainer::NetworkTypeMismatch, tr("Network type mismatch (Mainnet / Testnet)"));
         return;
      }

      // BIP 150/151 should be be complete by this point.
      isReady_ = true;
      emit authenticated();
   } else {
      emit PacketReceived(packet);
   }
}

void HeadlessListener::OnConnected()
{
   if (isConnected_) {
      logger_->error("already connected");
      return;
   }

   isConnected_ = true;
   logger_->debug("[HeadlessListener] Connected");
   emit connected();
}

void HeadlessListener::OnDisconnected()
{
   if (isShuttingDown_) {
      return;
   }

   SPDLOG_LOGGER_ERROR(logger_, "remote signer disconnected unexpectedly");
   isConnected_ = false;
   isReady_ = false;
   tryEmitError(HeadlessContainer::SocketFailed, tr("TCP connection was closed unexpectedly"));
}

void HeadlessListener::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   logger_->debug("[HeadlessListener] error {}", errorCode);
   isConnected_ = false;
   isReady_ = false;

   switch (errorCode) {
      case NoError:
         assert(false);
         break;
      case UndefinedSocketError:
         tryEmitError(HeadlessContainer::SocketFailed, tr("Socket error"));
         break;
      case HostNotFoundError:
         tryEmitError(HeadlessContainer::HostNotFound, tr("Host not found"));
         break;
      case HandshakeFailed:
         tryEmitError(HeadlessContainer::HandshakeFailed, tr("Handshake failed"));
         break;
      case SerializationFailed:
         tryEmitError(HeadlessContainer::SerializationFailed, tr("Serialization failed"));
         break;
      case HeartbeatWaitFailed:
         tryEmitError(HeadlessContainer::HeartbeatWaitFailed, tr("Connection lost"));
         break;
      case ConnectionTimeout:
         tryEmitError(HeadlessContainer::ConnectionTimeout, tr("Connection timeout"));
         break;
   }
}

bs::signer::RequestId HeadlessListener::Send(headless::RequestPacket packet, bool updateId)
{
   if (!connection_) {
      return 0;
   }

   bs::signer::RequestId id = 0;
   if (updateId) {
      id = newRequestId();
      packet.set_id(id);
   }

   if (!connection_->send(packet.SerializeAsString())) {
      logger_->error("[HeadlessListener] Failed to send request packet");
      emit disconnected();
      return 0;
   }
   return id;
}

HeadlessContainer::HeadlessContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
   : SignContainer(logger, opMode)
{
   qRegisterMetaType<headless::RequestPacket>();
   qRegisterMetaType<std::shared_ptr<bs::sync::hd::Leaf>>();
}

bs::signer::RequestId HeadlessContainer::Send(const headless::RequestPacket &packet, bool incSeqNo)
{
   if (!listener_) {
      return 0;
   }
   return listener_->Send(packet, incSeqNo);
}

void HeadlessContainer::ProcessSignTXResponse(unsigned int id, const std::string &data)
{
   headless::SignTxReply response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse SignTxReply");
      emit TXSigned(id, {}, bs::error::ErrorCode::FailedToParse);
      return;
   }
   emit TXSigned(id, response.signedtx(), static_cast<bs::error::ErrorCode>(response.errorcode()));
}

void HeadlessContainer::ProcessSettlementSignTXResponse(unsigned int id, const std::string &data)
{
   headless::SignTxReply response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbSettlementSignTxMap_.find(id);
   if (itCb == cbSettlementSignTxMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }

   if (itCb->second) {
      itCb->second(static_cast<bs::error::ErrorCode>(response.errorcode()), BinaryData(response.signedtx()));
   }
   cbSettlementSignTxMap_.erase(itCb);
}

void HeadlessContainer::ProcessCreateHDLeafResponse(unsigned int id, const std::string &data)
{
   headless::CreateHDLeafResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse CreateHDWallet reply");
      emit Error(id, "failed to parse");
      return;
   }

   bs::error::ErrorCode result = static_cast<bs::error::ErrorCode>(response.errorcode());
   if (result == bs::error::ErrorCode::NoError) {
      const auto path = bs::hd::Path::fromString(response.leaf().path());
      bs::core::wallet::Type leafType = bs::core::wallet::Type::Unknown;
      switch (static_cast<bs::hd::CoinType>(path.get(-2))) {
      case bs::hd::CoinType::Bitcoin_main:
      case bs::hd::CoinType::Bitcoin_test:
         leafType = bs::core::wallet::Type::Bitcoin;
         break;
      case bs::hd::CoinType::BlockSettle_Auth:
         leafType = bs::core::wallet::Type::Authentication;
         break;
      case bs::hd::CoinType::BlockSettle_CC:
         leafType = bs::core::wallet::Type::ColorCoin;
         break;
      }
      const auto leaf = std::make_shared<bs::sync::hd::Leaf>(response.leaf().walletid()
         , response.leaf().name(), response.leaf().desc(), this, logger_
         , leafType, response.leaf().extonly());
      logger_->debug("[HeadlessContainer] HDLeaf {} created", response.leaf().walletid());
      emit HDLeafCreated(id, leaf);
   }
   else {
      emit Error(id, bs::error::ErrorCodeToString(result).toStdString());
   }
}

void HeadlessContainer::ProcessGetHDWalletInfoResponse(unsigned int id, const std::string &data)
{
   headless::GetHDWalletInfoResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse GetHDWalletInfo reply");
      emit Error(id, "failed to parse");
      return;
   }
   if (response.error().empty()) {
      emit QWalletInfo(id, bs::hd::WalletInfo(response));
   }
   else {
      missingWallets_.insert(response.rootwalletid());
      emit Error(id, response.error());
   }
}

void HeadlessContainer::ProcessAutoSignActEvent(unsigned int id, const std::string &data)
{
   headless::AutoSignActEvent event;
   if (!event.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse SetLimits reply");
      emit Error(id, "failed to parse");
      return;
   }
   emit AutoSignStateChanged(event.rootwalletid(), event.autosignactive());
}

void HeadlessContainer::ProcessSetUserId(const std::string &data)
{
   headless::SetUserIdResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] failed to parse response", __func__);
      return;
   }
   if (!response.auth_wallet_id().empty() && (response.response() == headless::AWR_NoError)) {
      emit AuthLeafAdded(response.auth_wallet_id());
   }
   else {   // unset auth wallet
      emit AuthLeafAdded("");
   }
}

headless::SignTxRequest HeadlessContainer::createSignTxRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const SignContainer::PasswordType &password, bool keepDuplicatedRecipients)
{
   headless::SignTxRequest request;
   request.set_walletid(txSignReq.walletId);
   request.set_keepduplicatedrecipients(keepDuplicatedRecipients);

   if (txSignReq.populateUTXOs) {
      request.set_populateutxos(true);
   }

   for (const auto &utxo : txSignReq.inputs) {
      request.add_inputs(utxo.serialize().toBinStr());
   }

   for (const auto &recip : txSignReq.recipients) {
      request.add_recipients(recip->getSerializedScript().toBinStr());
   }
   if (txSignReq.fee) {
      request.set_fee(txSignReq.fee);
   }

   if (txSignReq.RBF) {
      request.set_rbf(true);
   }

   if (!password.isNull()) {
      request.set_password(password.toBinStr());
   }

   if (!txSignReq.prevStates.empty()) {
      request.set_unsignedstate(txSignReq.serializeState().toBinStr());
   }

   if (txSignReq.change.value) {
      auto change = request.mutable_change();
      change->set_address(txSignReq.change.address.display());
      change->set_index(txSignReq.change.index);
      change->set_value(txSignReq.change.value);
   }

   return  request;
}

bs::signer::RequestId HeadlessContainer::signTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , SignContainer::TXSignMode mode, const PasswordType& password
   , bool keepDuplicatedRecipients)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }

   headless::SignTxRequest request = createSignTxRequest(txSignReq, password, keepDuplicatedRecipients);

   headless::RequestPacket packet;
   switch (mode) {
   case TXSignMode::Full:
      packet.set_type(headless::SignTxRequestType);
      break;

   case TXSignMode::Partial:
      packet.set_type(headless::SignPartialTXRequestType);
      break;
   }
   packet.set_data(request.SerializeAsString());
   const auto id = Send(packet);
   signRequests_.insert(id);
   return id;
}

unsigned int HeadlessContainer::signPartialTXRequest(const bs::core::wallet::TXSignRequest &req
   , const PasswordType& password)
{
   return signTXRequest(req, TXSignMode::Partial, password);
}

bs::signer::RequestId HeadlessContainer::signPayoutTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const bs::Address &authAddr, const std::string &settlementId
   , const PasswordType& password)
{
   if ((txSignReq.inputs.size() != 1) || (txSignReq.recipients.size() != 1) || settlementId.empty()) {
      logger_->error("[HeadlessContainer] Invalid PayoutTXSignRequest");
      return 0;
   }
   headless::SignPayoutTXRequest request;
   request.set_input(txSignReq.inputs[0].serialize().toBinStr());
   request.set_recipient(txSignReq.recipients[0]->getSerializedScript().toBinStr());
   request.set_authaddress(authAddr.display());
   request.set_settlementid(settlementId);
//   if (autoSign) {
//      request.set_applyautosignrules(autoSign);
//   }

   if (!password.isNull()) {
      request.set_password(password.toBinStr());
   }

   headless::RequestPacket packet;
   packet.set_type(headless::SignPayoutTXRequestType);
   packet.set_data(request.SerializeAsString());
   const auto id = Send(packet);
   signRequests_.insert(id);
   return id;
}

bs::signer::RequestId HeadlessContainer::signSettlementTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const bs::sync::PasswordDialogData &dialogData, SignContainer::TXSignMode mode
   , bool keepDuplicatedRecipients
   , const std::function<void (bs::error::ErrorCode result, const BinaryData &signedTX)> &cb)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }

   headless::SignTxRequest signTxRequest = createSignTxRequest(txSignReq, {}, keepDuplicatedRecipients);

   headless::SignSettlementTxRequest settlementRequest;
   *(settlementRequest.mutable_signtxrequest()) = signTxRequest;
   *(settlementRequest.mutable_passworddialogdata()) = dialogData.toProtobufMessage();

   headless::RequestPacket packet;
   packet.set_type(headless::SignSettlementTxRequestType);

   packet.set_data(settlementRequest.SerializeAsString());
   const auto reqId = Send(packet);
   cbSettlementSignTxMap_[reqId] = cb;
   return reqId;
}

bs::signer::RequestId HeadlessContainer::signSettlementPartialTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const bs::sync::PasswordDialogData &dialogData
   , const std::function<void (bs::error::ErrorCode, const BinaryData &)> &cb)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }

   headless::SignTxRequest signTxRequest = createSignTxRequest(txSignReq, {});

   headless::SignSettlementTxRequest settlementRequest;
   *(settlementRequest.mutable_signtxrequest()) = signTxRequest;
   *(settlementRequest.mutable_passworddialogdata()) = dialogData.toProtobufMessage();

   headless::RequestPacket packet;
   packet.set_type(headless::SignSettlementPartialTxRequestType);
   packet.set_data(settlementRequest.SerializeAsString());

   const auto reqId = Send(packet);
   cbSettlementSignTxMap_[reqId] = cb;
   return reqId;
}

bs::signer::RequestId HeadlessContainer::signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const bs::sync::PasswordDialogData &dialogData, const bs::Address &authAddr, const std::string &settlementId
   , const std::function<void (bs::error::ErrorCode, const BinaryData &)> &cb)
{
   if ((txSignReq.inputs.size() != 1) || (txSignReq.recipients.size() != 1) || settlementId.empty()) {
      logger_->error("[HeadlessContainer] Invalid PayoutTXSignRequest");
      return 0;
   }
   headless::SignPayoutTXRequest request;
   request.set_input(txSignReq.inputs[0].serialize().toBinStr());
   request.set_recipient(txSignReq.recipients[0]->getSerializedScript().toBinStr());
   request.set_authaddress(authAddr.display());
   request.set_settlementid(settlementId);
//   if (autoSign) {
//      request.set_applyautosignrules(autoSign);
//   }

   headless::SignSettlementPayoutTxRequest settlementRequest;
   *(settlementRequest.mutable_signpayouttxrequest()) = request;
   *(settlementRequest.mutable_passworddialogdata()) = dialogData.toProtobufMessage();

   headless::RequestPacket packet;
   packet.set_type(headless::SignPayoutTXRequestType);
   packet.set_data(request.SerializeAsString());

   const auto reqId = Send(packet);
   cbSettlementSignTxMap_[reqId] = cb;
   return reqId;
}

bs::signer::RequestId HeadlessContainer::signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &txMultiReq)
{
   if (!txMultiReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXMultiSignRequest");
      return 0;
   }

   Signer signer;
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);

   headless::SignTXMultiRequest request;
   for (const auto &input : txMultiReq.inputs) {
      request.add_walletids(input.second);
      signer.addSpender(std::make_shared<ScriptSpender>(input.first));
   }
   for (const auto &recip : txMultiReq.recipients) {
      signer.addRecipient(recip);
   }
   request.set_signerstate(signer.serializeState().toBinStr());

   headless::RequestPacket packet;
   packet.set_type(headless::SignTXMultiRequestType);
   packet.set_data(request.SerializeAsString());
   const auto id = Send(packet);
   signRequests_.insert(id);
   return id;
}

bs::signer::RequestId HeadlessContainer::CancelSignTx(const BinaryData &txId)
{
   headless::CancelSignTx request;
   request.set_txid(txId.toBinStr());

   headless::RequestPacket packet;
   packet.set_type(headless::CancelSignTxRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::setUserId(const BinaryData &userId, const std::string &walletId)
{
   if (!listener_) {
      logger_->warn("[HeadlessContainer::SetUserId] listener not set yet");
      return 0;
   }

   bs::sync::PasswordDialogData info;
   info.setValue("WalletId", QString::fromStdString(walletId));

   headless::SetUserIdRequest request;
   auto dialogData = request.mutable_passworddialogdata();
   *dialogData = info.toProtobufMessage();
   if (!userId.isNull()) {
      request.set_userid(userId.toBinStr());
   }

   headless::RequestPacket packet;
   packet.set_type(headless::SetUserIdType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::syncCCNames(const std::vector<std::string> &ccNames)
{
   logger_->debug("[{}] syncing {} CCs", __func__, ccNames.size());
   headless::SyncCCNamesData request;
   for (const auto &cc : ccNames) {
      request.add_ccnames(cc);
   }

   headless::RequestPacket packet;
   packet.set_type(headless::SyncCCNamesType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &path
   , const std::vector<bs::wallet::PasswordData> &pwdData, const std::function<void(bs::error::ErrorCode result)> &cb)
{
   if (rootWalletId.empty() || (path.length() != 3)) {
      logger_->error("[HeadlessContainer] Invalid input data for HD wallet creation");
      return 0;
   }
   headless::CreateHDLeafRequest request;
   request.set_rootwalletid(rootWalletId);
   request.set_path(path.toString());

   bs::sync::PasswordDialogData info;
   info.setValue(QLatin1String("WalletId"), QString::fromStdString(rootWalletId));

   auto dialogData = request.mutable_passworddialogdata();
   *dialogData = info.toProtobufMessage();

   headless::RequestPacket packet;
   packet.set_type(headless::CreateHDLeafRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::DeleteHDRoot(const std::string &rootWalletId)
{
   if (rootWalletId.empty()) {
      return 0;
   }
   return SendDeleteHDRequest(rootWalletId, {});
}

bs::signer::RequestId HeadlessContainer::DeleteHDLeaf(const std::string &leafWalletId)
{
   if (leafWalletId.empty()) {
      return 0;
   }
   return SendDeleteHDRequest({}, leafWalletId);
}

bs::signer::RequestId HeadlessContainer::SendDeleteHDRequest(const std::string &rootWalletId, const std::string &leafId)
{
   headless::DeleteHDWalletRequest request;
   if (!rootWalletId.empty()) {
      request.set_rootwalletid(rootWalletId);
   }
   else if (!leafId.empty()) {
      request.set_leafwalletid(leafId);
   }
   else {
      logger_->error("[HeadlessContainer] can't send delete request - both IDs are empty");
      return 0;
   }

   headless::RequestPacket packet;
   packet.set_type(headless::DeleteHDWalletRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

//void HeadlessContainer::setLimits(const std::string &walletId, const SecureBinaryData &pass
//   , bool autoSign)
//{
//   if (walletId.empty()) {
//      logger_->error("[HeadlessContainer] no walletId for SetLimits");
//      return;
//   }
//   headless::SetLimitsRequest request;
//   request.set_rootwalletid(walletId);
//   if (!pass.isNull()) {
//      request.set_password(pass.toHexStr());
//   }
//   request.set_activateautosign(autoSign);

//   headless::RequestPacket packet;
//   packet.set_type(headless::SetLimitsRequestType);
//   packet.set_data(request.SerializeAsString());
//   Send(packet);
//}

bs::signer::RequestId HeadlessContainer::customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data)
{
   // serialize variant data
   QByteArray ba;
   QDataStream stream(&ba, QIODevice::WriteOnly);
   stream << data;

   headless::CustomDialogRequest request;
   request.set_dialogname(bs::signer::ui::getSignerDialogPath(signerDialog).toStdString());
   request.set_variantdata(ba.data(), size_t(ba.size()));

   headless::RequestPacket packet;
   packet.set_type(headless::ExecCustomDialogRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::GetInfo(const std::string &rootWalletId)
{
   if (rootWalletId.empty()) {
      return 0;
   }
   headless::GetHDWalletInfoRequest request;
   request.set_rootwalletid(rootWalletId);

   headless::RequestPacket packet;
   packet.set_type(headless::GetHDWalletInfoRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bool HeadlessContainer::isReady() const
{
   return (listener_ != nullptr) && listener_->isReady();
}

bool HeadlessContainer::isWalletOffline(const std::string &walletId) const
{
   return ((missingWallets_.find(walletId) != missingWallets_.end())
      || (woWallets_.find(walletId) != woWallets_.end()));
}

void HeadlessContainer::createSettlementWallet(const std::function<void(const std::shared_ptr<bs::sync::SettlementWallet> &)> &cb)
{
   headless::RequestPacket packet;
   packet.set_type(headless::CreateSettlWalletType);
   const auto reqId = Send(packet);
   cbSettlWalletMap_[reqId] = cb;
}

void HeadlessContainer::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   headless::RequestPacket packet;
   packet.set_type(headless::SyncWalletInfoType);
   const auto reqId = Send(packet);
   cbWalletInfoMap_[reqId] = cb;
}

void HeadlessContainer::syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &cb)
{
   headless::SyncWalletRequest request;
   request.set_walletid(id);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncHDWalletType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   cbHDWalletMap_[reqId] = cb;
}

void HeadlessContainer::syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &cb)
{
   headless::SyncWalletRequest request;
   request.set_walletid(id);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncWalletType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   cbWalletMap_[reqId] = cb;
}

void HeadlessContainer::syncAddressComment(const std::string &walletId, const bs::Address &addr
   , const std::string &comment)
{
   headless::SyncCommentRequest request;
   request.set_walletid(walletId);
   request.set_address(addr.display());
   request.set_comment(comment);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncCommentType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

void HeadlessContainer::syncTxComment(const std::string &walletId, const BinaryData &txHash
   , const std::string &comment)
{
   headless::SyncCommentRequest request;
   request.set_walletid(walletId);
   request.set_txhash(txHash.toBinStr());
   request.set_comment(comment);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncCommentType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

void HeadlessContainer::extendAddressChain(
   const std::string &walletId, unsigned count, bool extInt,
   const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &cb)
{
   headless::ExtendAddressChainRequest request;
   request.set_wallet_id(walletId);
   request.set_count(count);
   request.set_ext_int(extInt);

   headless::RequestPacket packet;
   packet.set_type(headless::ExtendAddressChainType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   if (!reqId) {
      if (cb) {
         cb({});
      }
      return;
   }
   cbExtAddrsMap_[reqId] = cb;
}

void HeadlessContainer::syncNewAddresses(const std::string &walletId
   , const std::vector<std::pair<std::string, AddressEntryType>> &inData
   , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &cb
   , bool persistent)
{
   headless::SyncNewAddressRequest request;
   request.set_wallet_id(walletId);
   for (const auto &in : inData) {
      auto addrData = request.add_addresses();
      addrData->set_index(in.first);
      addrData->set_aet(in.second);
   }

   headless::RequestPacket packet;
   packet.set_type(headless::SyncNewAddressType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   if (!reqId) {
      if (cb) {
         cb({});
      }
      return;
   }
   cbExtAddrsMap_[reqId] = cb;
}

void HeadlessContainer::syncAddressBatch(
   const std::string &walletId, const std::set<BinaryData>& addrSet,
   std::function<void(bs::sync::SyncState)> cb)
{
   QMetaObject::invokeMethod(this, [this, walletId, addrSet, cb] {
      headless::SyncAddressesRequest request;
      request.set_wallet_id(walletId);
      for (const auto &addr : addrSet) {
         request.add_addresses(addr.toBinStr());
      }

      headless::RequestPacket packet;
      packet.set_type(headless::SyncAddressesType);
      packet.set_data(request.SerializeAsString());
      const auto reqId = Send(packet);
      if (!reqId) {
         if (cb) {
            cb(bs::sync::SyncState::Failure);
         }
         return;
      }
      cbSyncAddrsMap_[reqId] = cb;
   });
}

static NetworkType mapFrom(headless::NetworkType netType)
{
   switch (netType) {
   case headless::MainNetType:   return NetworkType::MainNet;
   case headless::TestNetType:   return NetworkType::TestNet;
   default:    return NetworkType::Invalid;
   }
}

static bs::sync::WalletFormat mapFrom(headless::WalletFormat format)
{
   switch (format) {
   case headless::WalletFormatHD:         return bs::sync::WalletFormat::HD;
   case headless::WalletFormatPlain:      return bs::sync::WalletFormat::Plain;
   case headless::WalletFormatSettlement: return bs::sync::WalletFormat::Settlement;
   case headless::WalletFormatUnknown:
   default:    return bs::sync::WalletFormat::Unknown;
   }
}

void HeadlessContainer::ProcessSettlWalletCreate(unsigned int id, const std::string &data)
{
   headless::SettlWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbSettlWalletMap_.find(id);
   if (itCb == cbSettlWalletMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   const auto settlWallet = std::make_shared<bs::sync::SettlementWallet>(response.walletid()
      , response.name(), response.description(), this, logger_);
   itCb->second(settlWallet);
}

void HeadlessContainer::ProcessSyncWalletInfo(unsigned int id, const std::string &data)
{
   headless::SyncWalletInfoResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbWalletInfoMap_.find(id);
   if (itCb == cbWalletInfoMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   std::vector<bs::sync::WalletInfo> result;
   for (int i = 0; i < response.wallets_size(); ++i) {
      const auto walletInfo = response.wallets(i);
      result.push_back({ mapFrom(walletInfo.format()), walletInfo.id(), walletInfo.name()
         , walletInfo.description(), mapFrom(walletInfo.nettype()), walletInfo.watching_only() });
      if (walletInfo.watching_only()) {
         woWallets_.insert(walletInfo.id());
      }
   }
   itCb->second(result);
   cbWalletInfoMap_.erase(itCb);
}

void HeadlessContainer::ProcessSyncHDWallet(unsigned int id, const std::string &data)
{
   headless::SyncHDWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbHDWalletMap_.find(id);
   if (itCb == cbHDWalletMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   const bool isWoRoot = (woWallets_.find(response.walletid()) != woWallets_.end());
   bs::sync::HDWalletData result;
   for (int i = 0; i < response.groups_size(); ++i) {
      const auto groupInfo = response.groups(i);
      bs::sync::HDWalletData::Group group;
      group.type = static_cast<bs::hd::CoinType>(groupInfo.type());
      group.extOnly = groupInfo.ext_only();
      group.salt = groupInfo.salt();
      for (int j = 0; j < groupInfo.leaves_size(); ++j) {
         const auto leafInfo = groupInfo.leaves(j);
         if (isWoRoot) {
            woWallets_.insert(leafInfo.id());
         }
         group.leaves.push_back({ leafInfo.id(), leafInfo.index() });
      }
      result.groups.push_back(group);
   }
   itCb->second(result);
   cbHDWalletMap_.erase(itCb);
}

static bs::wallet::EncryptionType mapFrom(headless::EncryptionType encType)
{
   switch (encType) {
   case headless::EncryptionTypePassword: return bs::wallet::EncryptionType::Password;
   case headless::EncryptionTypeAutheID:  return bs::wallet::EncryptionType::Auth;
   case headless::EncryptionTypeUnencrypted:
   default:    return bs::wallet::EncryptionType::Unencrypted;
   }
}

void HeadlessContainer::ProcessSyncWallet(unsigned int id, const std::string &data)
{
   headless::SyncWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbWalletMap_.find(id);
   if (itCb == cbWalletMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }

   bs::sync::WalletData result;
   for (int i = 0; i < response.encryptiontypes_size(); ++i) {
      const auto encType = response.encryptiontypes(i);
      result.encryptionTypes.push_back(mapFrom(encType));
   }
   for (int i = 0; i < response.encryptionkeys_size(); ++i) {
      const auto encKey = response.encryptionkeys(i);
      result.encryptionKeys.push_back(encKey);
   }
   result.encryptionRank = { response.keyrank().m(), response.keyrank().n() };

   result.netType = mapFrom(response.nettype());
   result.highestExtIndex = response.highest_ext_index();
   result.highestIntIndex = response.highest_int_index();

   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto addrInfo = response.addresses(i);
      const bs::Address addr(addrInfo.address());
      if (addr.isNull()) {
         continue;
      }
      result.addresses.push_back({ addrInfo.index(), std::move(addr)
         , addrInfo.comment() });
   }
   for (int i = 0; i < response.addrpool_size(); ++i) {
      const auto addrInfo = response.addrpool(i);
      const bs::Address addr(addrInfo.address());
      if (addr.isNull()) {
         continue;
      }
      result.addrPool.push_back({ addrInfo.index(), std::move(addr), "" });
   }
   for (int i = 0; i < response.txcomments_size(); ++i) {
      const auto txInfo = response.txcomments(i);
      result.txComments.push_back({ txInfo.txhash(), txInfo.comment() });
   }
   itCb->second(result);
   cbWalletMap_.erase(itCb);
}

static bs::sync::SyncState mapFrom(headless::SyncState state)
{
   switch (state) {
   case headless::SyncState_Success:      return bs::sync::SyncState::Success;
   case headless::SyncState_NothingToDo:  return bs::sync::SyncState::NothingToDo;
   case headless::SyncState_Failure:      return bs::sync::SyncState::Failure;
   }
   return bs::sync::SyncState::Failure;
}

void HeadlessContainer::ProcessSyncAddresses(unsigned int id, const std::string &data)
{
   headless::SyncAddressesResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbSyncAddrsMap_.find(id);
   if (itCb == cbSyncAddrsMap_.end()) {
      logger_->error("[{}] no callback found for id {}", __func__, id);
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }

   const auto result = mapFrom(response.state());
   itCb->second(result);
   cbSyncAddrsMap_.erase(itCb);
}

void HeadlessContainer::ProcessExtAddrChain(unsigned int id, const std::string &data)
{
   headless::ExtendAddressChainResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbExtAddrsMap_.find(id);
   if (itCb == cbExtAddrsMap_.end()) {
      logger_->error("[{}] no callback found for id {}", __func__, id);
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   std::vector<std::pair<bs::Address, std::string>> result;
   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto &addr = response.addresses(i);
      result.push_back({ addr.address(), addr.index() });
   }
   itCb->second(result);
   cbExtAddrsMap_.erase(itCb);
}


RemoteSigner::RemoteSigner(const std::shared_ptr<spdlog::logger> &logger
   , const QString &host, const QString &port, NetworkType netType
   , const std::shared_ptr<ConnectionManager>& connectionManager
   , OpMode opMode
   , const bool ephemeralDataConnKeys
   , const std::string& ownKeyFileDir
   , const std::string& ownKeyFileName
   , const ZmqBIP15XDataConnection::cbNewKey& inNewKeyCB)
   : HeadlessContainer(logger, opMode)
   , host_(host), port_(port), netType_(netType)
   , ephemeralDataConnKeys_(ephemeralDataConnKeys)
   , ownKeyFileDir_(ownKeyFileDir)
   , ownKeyFileName_(ownKeyFileName)
   , cbNewKey_{inNewKeyCB}
   , connectionManager_{connectionManager}
{
}

// Establish the remote connection to the signer.
bool RemoteSigner::Start()
{
   if (!connection_) {
      RecreateConnection();
   }

   // If we've already connected, don't do more setup.
   if (headlessConnFinished_) {
      return true;
   }

   if (opMode() == OpMode::RemoteInproc) {
      connection_->SetZMQTransport(ZMQTransport::InprocTransport);
   }

   {
      std::lock_guard<std::mutex> lock(mutex_);
      listener_ = std::make_shared<HeadlessListener>(logger_, connection_, netType_);
      connect(listener_.get(), &HeadlessListener::connected, this
         , &RemoteSigner::onConnected, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::authenticated, this
         , &RemoteSigner::onAuthenticated, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::disconnected, this
         , &RemoteSigner::onDisconnected, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::error, this
         , &RemoteSigner::onConnError, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::PacketReceived, this
         , &RemoteSigner::onPacketReceived, Qt::QueuedConnection);
   }

   return Connect();
}

bool RemoteSigner::Stop()
{
   return Disconnect();
}

bool RemoteSigner::Connect()
{
   if (!connection_) {
      logger_->error("[{}] connection not created", __func__);
      return false;
   }

   if (connection_->isActive()) {
      return true;
   }

   listener_->wasErrorReported_ = false;
   listener_->isShuttingDown_ = false;

   bool result = connection_->openConnection(host_.toStdString(), port_.toStdString(), listener_.get());
   if (!result) {
      logger_->error("[HeadlessContainer] Failed to open connection to "
         "headless container");
      return false;
   }

   emit connected();
   headlessConnFinished_ = true;
   return true;
}

bool RemoteSigner::Disconnect()
{
   if (!connection_) {
      return true;
   }

   if (listener_) {
      listener_->isShuttingDown_ = true;
   }

   bool result = connection_->closeConnection();
   connection_.reset();
   return result;
}

void RemoteSigner::Authenticate()
{
   mutex_.lock();
   if (!listener_) {
      mutex_.unlock();
      emit connectionError(UnknownError, tr("listener missing on authenticate"));
      return;
   }
   mutex_.unlock();

   headless::AuthenticationRequest request;
   request.set_nettype((netType_ == NetworkType::TestNet) ? headless::TestNetType : headless::MainNetType);

   headless::RequestPacket packet;
   packet.set_type(headless::AuthenticationRequestType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

void RemoteSigner::RecreateConnection()
{
   logger_->info("[{}] Restart connection...", __func__);

   ZmqBIP15XDataConnectionParams params;
   params.ephemeralPeers = ephemeralDataConnKeys_;
   params.ownKeyFileDir = ownKeyFileDir_;
   params.ownKeyFileName = ownKeyFileName_;
   params.setLocalHeartbeatInterval();

   // Server's cookies are not available in remote mode
   if (opMode() == OpMode::Local || opMode() == OpMode::LocalInproc) {
      params.cookie = BIP15XCookie::ReadServer;
      params.cookiePath = SystemFilePaths::appDataLocation() + "/" + "signerServerID";
   }

   try {
      connection_ = connectionManager_->CreateZMQBIP15XDataConnection(params);
      connection_->setCBs(cbNewKey_);

      headlessConnFinished_ = false;
   }
   catch (const std::exception &e) {
      logger_->error("[{}] connection creation failed: {}", __func__, e.what());
      QTimer::singleShot(10, this, [this] {  // slight delay is required on start-up init
         emit connectionError(ConnectionError::SocketFailed, tr("Connection creation failed"));
      });
   }
}

void RemoteSigner::ScheduleRestart()
{
   if (isRestartScheduled_) {
      return;
   }

   isRestartScheduled_ = true;
   auto timeout = isLocal() ? kLocalReconnectPeriod : kRemoteReconnectPeriod;
   QTimer::singleShot(timeout, this, [this] {
      isRestartScheduled_ = false;
      RecreateConnection();
      Start();
   });
}

bool RemoteSigner::isOffline() const
{
   std::lock_guard<std::mutex> lock(mutex_);
   return (listener_ == nullptr);
}

void RemoteSigner::updatePeerKeys(const ZmqBIP15XPeers &peers)
{
   if (!connection_) {
      RecreateConnection();
   }

   connection_->updatePeerKeys(peers);
}

void RemoteSigner::onConnected()
{
   Authenticate();
}

void RemoteSigner::onAuthenticated()
{
   // Once the BIP 150/151 handshake is complete, it's safe to start sending
   // app-level data to the signer.
   emit authenticated();
   emit ready();
}

void RemoteSigner::onDisconnected()
{
   missingWallets_.clear();
   woWallets_.clear();

   // signRequests_ will be empty after that
   std::set<bs::signer::RequestId> tmpReqs = std::move(signRequests_);

   for (const auto &id : tmpReqs) {
      emit TXSigned(id, {}, bs::error::ErrorCode::TxCanceled, "Signer disconnected");
   }

   emit disconnected();

   ScheduleRestart();
}

void RemoteSigner::onConnError(ConnectionError error, const QString &details)
{
   emit connectionError(error, details);
   ScheduleRestart();
}

void RemoteSigner::onPacketReceived(headless::RequestPacket packet)
{
   signRequests_.erase(packet.id());

   switch (packet.type()) {
   case headless::SignTxRequestType:
   case headless::SignPartialTXRequestType:
   case headless::SignPayoutTXRequestType:
   case headless::SignTXMultiRequestType:
      ProcessSignTXResponse(packet.id(), packet.data());
      break;

   case headless::SignSettlementTxRequestType:
      ProcessSettlementSignTXResponse(packet.id(), packet.data());
      break;

   case headless::CreateHDLeafRequestType:
      ProcessCreateHDLeafResponse(packet.id(), packet.data());
      break;

   case headless::GetHDWalletInfoRequestType:
      ProcessGetHDWalletInfoResponse(packet.id(), packet.data());
      break;

   case headless::SetUserIdType:
      ProcessSetUserId(packet.data());
      break;

   case headless::AutoSignActType:
      ProcessAutoSignActEvent(packet.id(), packet.data());
      break;

   case headless::CreateSettlWalletType:
      ProcessSettlWalletCreate(packet.id(), packet.data());
      break;

   case headless::SyncWalletInfoType:
      ProcessSyncWalletInfo(packet.id(), packet.data());
      break;

   case headless::SyncHDWalletType:
      ProcessSyncHDWallet(packet.id(), packet.data());
      break;

   case headless::SyncWalletType:
      ProcessSyncWallet(packet.id(), packet.data());
      break;

   case headless::SyncCommentType:
      break;   // normally no data will be returned on sync of comments

   case headless::SyncAddressesType:
      ProcessSyncAddresses(packet.id(), packet.data());
      break;

   case headless::ExtendAddressChainType:
   case headless::SyncNewAddressType:
      ProcessExtAddrChain(packet.id(), packet.data());
      break;

   case headless::WalletsListUpdatedType:
      logger_->debug("received WalletsListUpdatedType message");
      emit walletsListUpdated();
      break;

   default:
      logger_->warn("[HeadlessContainer] Unknown packet type: {}", packet.type());
      break;
   }
}

bs::signer::RequestId RemoteSigner::signTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , SignContainer::TXSignMode mode, const PasswordType& password
   , bool keepDuplicatedRecipients)
{
   if (isWalletOffline(txSignReq.walletId)) {
      return signOffline(txSignReq);
   }
   return HeadlessContainer::signTXRequest(txSignReq, mode, password, keepDuplicatedRecipients);
}

bs::signer::RequestId RemoteSigner::signOffline(const bs::core::wallet::TXSignRequest &txSignReq)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }

   Blocksettle::Storage::Signer::TXRequest request;
   request.set_walletid(txSignReq.walletId);

   for (const auto &utxo : txSignReq.inputs) {
      auto input = request.add_inputs();
      input->set_utxo(utxo.serialize().toBinStr());
      const auto addr = bs::Address::fromUTXO(utxo);
      input->mutable_address()->set_address(addr.display());
   }

   for (const auto &recip : txSignReq.recipients) {
      request.add_recipients(recip->getSerializedScript().toBinStr());
   }

   if (txSignReq.fee) {
      request.set_fee(txSignReq.fee);
   }
   if (txSignReq.RBF) {
      request.set_rbf(true);
   }

   if (txSignReq.change.value) {
      auto change = request.mutable_change();
      change->mutable_address()->set_address(txSignReq.change.address.display());
      change->mutable_address()->set_index(txSignReq.change.index);
      change->set_value(txSignReq.change.value);
   }

   if (!txSignReq.comment.empty()) {
      request.set_comment(txSignReq.comment);
   }

   Blocksettle::Storage::Signer::File fileContainer;
   auto container = fileContainer.add_payload();
   container->set_type(Blocksettle::Storage::Signer::RequestFileType);
   container->set_data(request.SerializeAsString());

   const auto reqId = listener_->newRequestId();
   const std::string &fileNamePath = txSignReq.offlineFilePath;

   QFile f(QString::fromStdString(fileNamePath));
   if (f.exists()) {
      txSignedAsync(reqId, {}, bs::error::ErrorCode::TxRequestFileExist, fileNamePath);
      return reqId;
   }
   if (!f.open(QIODevice::WriteOnly)) {
      txSignedAsync(reqId, {}, bs::error::ErrorCode::TxFailedToOpenRequestFile, fileNamePath);
      return reqId;
   }

   const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
   if (f.write(data) != data.size()) {
      txSignedAsync(reqId, {}, bs::error::ErrorCode::TxFailedToWriteRequestFile, fileNamePath);
      return reqId;
   }
   f.close();

   // response should be async
   txSignedAsync(reqId, {}, bs::error::ErrorCode::NoError);
   return reqId;
}

void RemoteSigner::txSignedAsync(bs::signer::RequestId id, const BinaryData &signedTX, bs::error::ErrorCode result, const std::string &errorReason)
{
   QMetaObject::invokeMethod(this, [this, id, signedTX, result, errorReason] {
      emit TXSigned(id, signedTX, result, errorReason);
   }, Qt::QueuedConnection);
}

LocalSigner::LocalSigner(const std::shared_ptr<spdlog::logger> &logger
   , const QString &homeDir, NetworkType netType, const QString &port
   , const std::shared_ptr<ConnectionManager>& connectionManager
   , const bool startSignerProcess
   , const std::string& ownKeyFileDir
   , const std::string& ownKeyFileName
   , double asSpendLimit
   , const ZmqBIP15XDataConnection::cbNewKey& inNewKeyCB)
   : RemoteSigner(logger, QLatin1String("127.0.0.1"), port, netType
      , connectionManager, OpMode::Local, true
      , ownKeyFileDir, ownKeyFileName, inNewKeyCB)
      , homeDir_(homeDir), startProcess_(startSignerProcess), asSpendLimit_(asSpendLimit)
{}

LocalSigner::~LocalSigner() noexcept
{
   Stop();
}

QStringList LocalSigner::args() const
{
   auto walletsCopyDir = homeDir_ + QLatin1String("/copy");
   if (!QDir().exists(walletsCopyDir)) {
      walletsCopyDir = homeDir_ + QLatin1String("/signer");
   }

   QStringList result;
   result << QLatin1String("--guimode") << QLatin1String("litegui");
   switch (netType_) {
   case NetworkType::TestNet:
   case NetworkType::RegTest:
      result << QString::fromStdString("--testnet");
      break;
   case NetworkType::MainNet:
      result << QString::fromStdString("--mainnet");
      break;
   default:
      break;
   }

   // Among many other things, send the signer the terminal's BIP 150 ID key.
   // Processes reading keys from the disk are subject to attack.
   result << QLatin1String("--listen") << QLatin1String("127.0.0.1");
   result << QLatin1String("--accept_from") << QLatin1String("127.0.0.1");
   result << QLatin1String("--port") << port_;
   result << QLatin1String("--dirwallets") << walletsCopyDir;
   if (asSpendLimit_ > 0) {
      result << QLatin1String("--auto_sign_spend_limit")
         << QString::number(asSpendLimit_, 'f', 8);
   }
   result << QLatin1String("--terminal_id_key")
      << QString::fromStdString(connection_->getOwnPubKey().toHexStr());

   return result;
}

bool LocalSigner::Start()
{
   Stop();

   bool result = RemoteSigner::Start();
   if (!result) {
      return false;
   }

   if (startProcess_) {
      // If there's a previous headless process, stop it.
      headlessProcess_ = std::make_shared<QProcess>();

#ifdef Q_OS_WIN
      const auto signerAppPath = QCoreApplication::applicationDirPath() + QLatin1String("/blocksettle_signer.exe");
#elif defined (Q_OS_MACOS)
      auto bundleDir = QDir(QCoreApplication::applicationDirPath());
      bundleDir.cdUp();
      bundleDir.cdUp();
      bundleDir.cdUp();
      const auto signerAppPath = bundleDir.absoluteFilePath(QLatin1String("blocksettle_signer"));
#else
      const auto signerAppPath = QCoreApplication::applicationDirPath() + QLatin1String("/blocksettle_signer");
#endif
      if (!QFile::exists(signerAppPath)) {
         logger_->error("[HeadlessContainer] Signer binary {} not found"
            , signerAppPath.toStdString());
         emit connectionError(UnknownError, tr("missing signer binary"));
         return false;
      }

      const auto cmdArgs = args();
      logger_->debug("[HeadlessContainer] starting {} {}"
         , signerAppPath.toStdString(), cmdArgs.join(QLatin1Char(' ')).toStdString());

#ifndef NDEBUG
      headlessProcess_->setProcessChannelMode(QProcess::MergedChannels);
      connect(headlessProcess_.get(), &QProcess::readyReadStandardOutput, this, [this]() {
         qDebug().noquote() << headlessProcess_->readAllStandardOutput();
      });
#endif

      headlessProcess_->start(signerAppPath, cmdArgs);
      if (!headlessProcess_->waitForStarted(kStartTimeout)) {
         logger_->error("[HeadlessContainer] Failed to start process");
         headlessProcess_.reset();
         emit connectionError(UnknownError, tr("failed to start process"));
         return false;
      }
   }

   return true;
}

bool LocalSigner::Stop()
{
   RemoteSigner::Stop();

   if (headlessProcess_) {
      if (!headlessProcess_->waitForFinished(kKillTimeout)) {
         headlessProcess_->terminate();
         headlessProcess_->waitForFinished(kKillTimeout);
      }
      headlessProcess_.reset();
   }
   return true;
}

void HeadlessListener::processDisconnectNotification()
{
   SPDLOG_LOGGER_INFO(logger_, "remote signer has been disconnected");
   isConnected_ = false;
   isReady_ = false;
   tryEmitError(HeadlessContainer::SignerGoesOffline, tr("Remote signer disconnected"));
}

void HeadlessListener::tryEmitError(SignContainer::ConnectionError errorCode, const QString &msg)
{
   // Try to send error only once because only first error should be relevant.
   if (!wasErrorReported_) {
      wasErrorReported_ = true;
      emit error(errorCode, msg);
   }
}
