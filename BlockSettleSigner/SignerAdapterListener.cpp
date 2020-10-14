/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignerAdapterListener.h"

#include <spdlog/spdlog.h>

#include "BSErrorCode.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "DispatchQueue.h"
#include "HeadlessApp.h"
#include "HeadlessContainerListener.h"
#include "OfflineSigner.h"
#include "ProtobufHeadlessUtils.h"
#include "ScopeGuard.h"
#include "ServerConnection.h"
#include "Settings/HeadlessSettings.h"
#include "StringUtils.h"
#include "SystemFileUtils.h"

#include "bs_signer.pb.h"
#include "headless.pb.h"

using namespace Blocksettle::Communication;

class HeadlessContainerCallbacksImpl : public HeadlessContainerCallbacks
{
public:
   HeadlessContainerCallbacksImpl(SignerAdapterListener *owner)
      : owner_(owner)
   {}

   void clientConn(const std::string &clientId, const ServerConnectionListener::Details &details) override
   {
      signer::ClientConnected evt;
      evt.set_client_id(clientId);
      auto ipAddrIt = details.find(ServerConnectionListener::Detail::IpAddr);
      if (ipAddrIt != details.end()) {
         evt.set_ip_address(ipAddrIt->second);
      }
      auto pubKeyIt = details.find(ServerConnectionListener::Detail::PublicKey);
      if (pubKeyIt != details.end()) {
         evt.set_public_key(pubKeyIt->second);
      }
      owner_->sendData(signer::PeerConnectedType, evt.SerializeAsString());
   }

   void clientDisconn(const std::string &clientId) override
   {
      signer::ClientDisconnected evt;
      evt.set_client_id(clientId);
      owner_->sendData(signer::PeerDisconnectedType, evt.SerializeAsString());

      if (owner_->settings_->runMode() == bs::signer::RunMode::litegui) {
         owner_->logger_->info("Quit because terminal disconnected unexpectedly and litegui used");
         owner_->queue_->quit();
      }
   }

   void ccNamesReceived(bool result) override
   {
      signer::TerminalEvent evt;
      evt.set_cc_info_received(result);
      owner_->sendData(signer::TerminalEventType, evt.SerializeAsString());
   }

   void txSigned(const BinaryData &tx) override
   {
      signer::SignTxEvent evt;
      evt.set_signedtx(tx.toBinStr());
      owner_->sendData(signer::TxSignedType, evt.SerializeAsString());
   }

   void cancelTxSign(const BinaryData &txId) override
   {
      headless::CancelSignTx evt;
      evt.set_tx_id(txId.toBinStr());
      owner_->sendData(signer::CancelTxSignType, evt.SerializeAsString());
   }

   void autoSignActivated(bool active, const std::string &walletId) override
   {
      signer::AutoSignActResponse evt;
      evt.set_rootwalletid(walletId);
      evt.set_errorcode(static_cast<uint32_t>(active ? bs::error::ErrorCode::NoError
         : bs::error::ErrorCode::AutoSignDisabled));
      owner_->sendData(signer::AutoSignActType, evt.SerializeAsString());
   }

   void xbtSpent(uint64_t value, bool autoSign) override
   {
      signer::XbtSpentEvent evt;
      evt.set_value(value);
      evt.set_auto_sign(autoSign);
      owner_->sendData(signer::XbtSpentType, evt.SerializeAsString());
   }

   void customDialog(const std::string &dialogName, const std::string &data) override
   {
      signer::CustomDialogRequest evt;
      evt.set_dialogname(dialogName);
      evt.set_variantdata(data);
      owner_->sendData(signer::ExecCustomDialogRequestType, evt.SerializeAsString());
   }

   void terminalHandshakeFailed(const std::string &peerAddress) override
   {
      signer::TerminalEvent evt;
      evt.set_peer_address(peerAddress);
      evt.set_handshake_ok(false);
      owner_->sendData(signer::TerminalEventType, evt.SerializeAsString());
   }

   void decryptWalletRequest(signer::PasswordDialogType dialogType
      , const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData
      , const bs::core::wallet::TXSignRequest &txReq = {}) override
   {
      signer::DecryptWalletRequest request;
      request.set_dialogtype(dialogType);
      *(request.mutable_signtxrequest()) = bs::signer::coreTxRequestToPb(txReq);
      *(request.mutable_passworddialogdata()) = dialogData;

      owner_->sendData(signer::DecryptWalletRequestType, request.SerializeAsString());
   }

   void updateDialogData(const Internal::PasswordDialogDataWrapper &dialogData) override
   {
      headless::UpdateDialogDataRequest request;
      *request.mutable_passworddialogdata() = dialogData;
      owner_->sendData(signer::UpdateDialogDataType, request.SerializeAsString());
   }

   void walletChanged(const std::string &walletId) override
   {
      signer::UpdateWalletRequest request;
      request.set_wallet_id(walletId);
      owner_->sendData(signer::UpdateWalletType, request.SerializeAsString());
   }

   SignerAdapterListener *owner_{};
};

SignerAdapterListener::SignerAdapterListener(HeadlessAppObj *app
   , const std::weak_ptr<ServerConnection> &connection
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::core::WalletsManager> &walletsMgr
   , const std::shared_ptr<DispatchQueue> &queue
   , const std::shared_ptr<HeadlessSettings> &settings)
   : ServerConnectionListener(), app_(app)
   , connection_(connection)
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , queue_(queue)
   , settings_(settings)
   , callbacks_(new HeadlessContainerCallbacksImpl(this))
{}

SignerAdapterListener::~SignerAdapterListener() noexcept = default;

void SignerAdapterListener::OnDataFromClient(const std::string &clientId, const std::string &data)
{
   queue_->dispatch([this, clientId, data] {
      // Process all data on main thread (no need to worry about data races)
      processData(clientId, data);
   });
}

void SignerAdapterListener::OnClientConnected(const std::string &clientId, const Details &details)
{
   logger_->debug("[SignerAdapterListener] client {} connected", bs::toHex(clientId));
}

void SignerAdapterListener::OnClientDisconnected(const std::string &clientId)
{
   logger_->debug("[SignerAdapterListener] client {} disconnected", bs::toHex(clientId));

   shutdownIfNeeded();
}

void SignerAdapterListener::onClientError(const std::string &clientId, ClientError error, const Details &details)
{
   logger_->debug("[SignerAdapterListener] client {} error: {}", bs::toHex(clientId), error);

   shutdownIfNeeded();
}

void SignerAdapterListener::processData(const std::string &clientId, const std::string &data)
{
   signer::Packet packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request packet", __func__);
      return;
   }
   bool rc = false;
   switch (packet.type()) {
   case::signer::HeadlessReadyType:
      rc = sendReady();
      break;
   case signer::SignOfflineTxRequestType:
      rc = onSignOfflineTxRequest(packet.data(), packet.id());
      break;
   case signer::SyncWalletInfoType:
      rc = onSyncWalletInfo(packet.id());
      break;
   case signer::SyncHDWalletType:
      rc = onSyncHDWallet(packet.data(), packet.id());
      break;
   case signer::SyncWalletType:
      rc = onSyncWallet(packet.data(), packet.id());
      break;
   case signer::GetDecryptedNodeType:
      rc = onGetDecryptedNode(packet.data(), packet.id());
      break;
   case signer::SetLimitsType:
      rc = onSetLimits(packet.data());
      break;
   case signer::PasswordReceivedType:
      rc = onPasswordReceived(packet.data());
      break;
   case signer::RequestCloseType:
      rc = onRequestClose();
      break;
   case signer::AutoSignActType:
      rc = onAutoSignRequest(packet.data(), packet.id());
      break;
   case signer::ChangePasswordType:
      rc = onChangePassword(packet.data(), packet.id());
      break;
   case signer::CreateHDWalletType:
      rc = onCreateHDWallet(packet.data(), packet.id());
      break;
   case signer::DeleteHDWalletType:
      rc = onDeleteHDWallet(packet.data(), packet.id());
      break;
   case signer::ImportWoWalletType:
      rc = onImportWoWallet(packet.data(), packet.id());
      break;
   case signer::ImportHwWalletType:
      rc = onImportHwWallet(packet.data(), packet.id());
      break;
   case signer::ExportWoWalletType:
      rc = onExportWoWallet(packet.data(), packet.id());
      break;
   case signer::SyncSettingsRequestType:
      rc = onSyncSettings(packet.data());
      break;
   case signer::ControlPasswordReceivedType:
      rc = onControlPasswordReceived(packet.data());
      break;
   case signer::ChangeControlPasswordType:
      rc = onChangeControlPassword(packet.data(), packet.id());
      break;
   case signer::WindowStatusType:
      rc = onWindowsStatus(packet.data(), packet.id());
      break;
   case signer::VerifyOfflineTxRequestType:
      rc = onVerifyOfflineTx(packet.data(), packet.id());
      break;
   default:
      logger_->warn("[SignerAdapterListener::{}] unprocessed packet type {}", __func__, packet.type());
      break;
   }
   if (!rc) {
      sendData(packet.type(), "", packet.id());
   }
}

bool SignerAdapterListener::sendData(signer::PacketType pt, const std::string &data
   , bs::signer::RequestId reqId)
{
   auto connection = connection_.lock();
   if (!connection) {
      return false;
   }

   signer::Packet packet;
   packet.set_type(pt);
   packet.set_data(data);
   if (reqId) {
      packet.set_id(reqId);
   }

   return connection->SendDataToAllClients(packet.SerializeAsString());
}

void SignerAdapterListener::sendStatusUpdate()
{
   signer::UpdateStatus evt;
   evt.set_signer_bind_status(signer::BindStatus(app_->signerBindStatus()));
   evt.set_signer_pub_key(app_->signerPubKey().toBinStr());
   sendData(signer::UpdateStatusType, evt.SerializeAsString());

   callbacks_->ccNamesReceived(!walletsMgr_->ccLeaves().empty());
}

void SignerAdapterListener::sendControlPasswordStatusUpdate(const signer::ControlPasswordStatus &status)
{
   signer::UpdateControlPasswordStatus evt;
   evt.set_controlpasswordstatus(status);
   sendData(signer::UpdateControlPasswordStatusType, evt.SerializeAsString());
}

void SignerAdapterListener::resetConnection()
{
   connection_.reset();
}

HeadlessContainerCallbacks *SignerAdapterListener::callbacks() const
{
   return callbacks_.get();
}

bool SignerAdapterListener::onSignOfflineTxRequest(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SignOfflineTxRequest request;
   signer::SignTxEvent evt;

   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      evt.set_errorcode((int)bs::error::ErrorCode::TxInvalidRequest);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
   }

   bs::core::wallet::TXSignRequest txSignReq = bs::signer::pbTxRequestToCore(request.tx_request(), logger_);

   auto errorCode = verifyOfflineSignRequest(txSignReq);
   if (errorCode != bs::error::ErrorCode::NoError) {
      SPDLOG_LOGGER_ERROR(logger_, "offline sign verification failed");
      evt.set_errorcode(static_cast<uint32_t>(errorCode));
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
   }

   // implication: all input leaves should belong to one hdWallet
   const auto hdWallet = walletsMgr_->getHDRootForLeaf(txSignReq.walletIds.front());
   try {
      if (txSignReq.walletIds.size() == 1) {
         bs::core::WalletPasswordScoped lock(hdWallet, SecureBinaryData::fromString(request.password()));
         BinaryData signedTx = hdWallet->signTXRequestWithWallet(txSignReq);
         evt.set_signedtx(signedTx.toBinStr());
      }
      else {
         bs::core::wallet::TXMultiSignRequest multiReq;
         multiReq.armorySigner_.merge(txSignReq.armorySigner_);
         multiReq.RBF = txSignReq.RBF;

         bs::core::WalletMap wallets;
         for (unsigned i=0; i<txSignReq.armorySigner_.getTxInCount(); i++) {
            auto spender = txSignReq.armorySigner_.getSpender(i);
            const auto addr = bs::Address::fromScript(spender->getOutputScript());
            const auto wallet = walletsMgr_->getWalletByAddress(addr);
            if (!wallet) {
               logger_->error("[{}] failed to find wallet for input address {}"
                  , __func__, addr.display());
               evt.set_errorcode((int)bs::error::ErrorCode::WrongAddress);
               return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
            }
            multiReq.addWalletId(wallet->walletId());
            wallets[wallet->walletId()] = wallet;
         }
         {
            const bs::core::WalletPasswordScoped passLock(hdWallet, SecureBinaryData::fromString(request.password()));
            const auto tx = bs::core::SignMultiInputTX(multiReq, wallets);
            evt.set_signedtx(tx.toBinStr());
         }
      }
      evt.set_errorcode((int)bs::error::ErrorCode::NoError);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
   }
   catch (const std::exception &e) {
      logger_->error("[SignerAdapterListener::{}] sign error: {}"
         , __func__, e.what());
   }
   evt.set_errorcode((int)bs::error::ErrorCode::InvalidPassword);
   return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onSyncWalletInfo(bs::signer::RequestId reqId)
{
   headless::SyncWalletInfoResponse response = bs::sync::exportHDWalletsInfoToPbMessage(walletsMgr_);
   return sendData(signer::SyncWalletInfoType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onSyncHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SyncWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (hdWallet) {
      signer::SyncHDWalletResponse response;
      for (const auto &group : hdWallet->getGroups()) {
         auto groupEntry = response.add_groups();
         groupEntry->set_type(static_cast<bs::hd::CoinType>(group->index()));

         for (const auto &leaf : group->getAllLeaves()) {
            auto leafEntry = groupEntry->add_leaves();
            leafEntry->set_id(leaf->walletId());
            leafEntry->set_path(leaf->path().toString());

            if (groupEntry->type() == bs::hd::CoinType::BlockSettle_Settlement) {
               const auto settlLeaf = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leaf);
               if (settlLeaf == nullptr) {
                  throw std::runtime_error("unexpected leaf type");
               }
               const auto rootAsset = settlLeaf->getRootAsset();
               const auto rootSingle = std::dynamic_pointer_cast<AssetEntry_Single>(rootAsset);
               if (rootSingle == nullptr) {
                  throw std::runtime_error("invalid root asset");
               }
               const auto authAddr = BtcUtils::getHash160(rootSingle->getPubKey()->getCompressedKey());
               leafEntry->set_extra_data(authAddr.toBinStr());
            }
         }
      }
      return sendData(signer::SyncHDWalletType, response.SerializeAsString(), reqId);
   } else {
      logger_->error("[SignerAdapterListener::{}] failed to find HD wallet with id {}"
         , __func__, request.wallet_id());
   }
   return false;
}

bool SignerAdapterListener::onSyncWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SyncWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.wallet_id());
   if (!wallet) {
      logger_->error("[SignerAdapterListener::{}] failed to find wallet with id {}"
         , __func__, request.wallet_id());
      return false;
   }
   const auto rootWallet = walletsMgr_->getHDRootForLeaf(wallet->walletId());
   if (!rootWallet) {
      logger_->error("[SignerAdapterListener::{}] failed to find root wallet for leaf {}"
         , __func__, request.wallet_id());
      return false;
   }

   signer::SyncWalletResponse response;
   response.set_wallet_id(wallet->walletId());
   response.set_highest_ext_index(wallet->getExtAddressCount());
   response.set_highest_int_index(wallet->getIntAddressCount());

   for (const auto &addr : wallet->getUsedAddressList()) {
      const auto index = wallet->getAddressIndex(addr);
      auto address = response.add_addresses();
      address->set_address(addr.display());
      address->set_index(index);
   }
   return sendData(signer::SyncWalletType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::sendWoWallet(const std::shared_ptr<bs::core::hd::Wallet> &wallet
   , Blocksettle::Communication::signer::PacketType pt, bs::signer::RequestId reqId)
{
   signer::CreateWatchingOnlyResponse response;
   response.set_wallet_id(wallet->walletId());
   response.set_name(wallet->name());
   response.set_description(wallet->description());

   for (const auto &group : wallet->getGroups()) {
      auto groupEntry = response.add_groups();
      groupEntry->set_type(group->index());
      for (const auto &leaf : group->getAllLeaves()) {
         auto leafEntry = groupEntry->add_leaves();
         leafEntry->set_id(leaf->walletId());
         leafEntry->set_path(leaf->path().toString());
         for (const auto &addr : leaf->getUsedAddressList()) {
            auto addrEntry = leafEntry->add_addresses();
            addrEntry->set_index(leaf->getAddressIndex(addr));
            addrEntry->set_aet(addr.getType());
         }
      }
   }
   return sendData(pt, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onGetDecryptedNode(const std::string &data, bs::signer::RequestId reqId)
{
   signer::DecryptWalletEvent request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (!hdWallet) {
      logger_->error("[SignerAdapterListener::{}] failed to find HD wallet with id {}"
         , __func__, request.wallet_id());
      return false;
   }

   std::string seedStr, privKeyStr;

   try {
      const bs::core::WalletPasswordScoped lock(hdWallet, SecureBinaryData::fromString(request.password()));
      const auto &seed = hdWallet->getDecryptedSeed();
      seedStr = seed.seed().toBinStr();
      privKeyStr = seed.toXpriv().toBinStr();
   }
   catch (const WalletException &e) {
      logger_->error("[SignerAdapterListener::onGetDecryptedNode] failed to decrypt wallet with id {}: {}"
         , request.wallet_id(), e.what());
      return false;
   }
   catch (const DecryptedDataContainerException &e) {
      logger_->error("[SignerAdapterListener::onGetDecryptedNode] wallet {} decryption failure: {}"
         , request.wallet_id(), e.what());
      return false;
   }
   catch (...) {
      logger_->error("[SignerAdapterListener::onGetDecryptedNode] wallet {} decryption error"
         , request.wallet_id());
      return false;
   }

   signer::DecryptedNodeResponse response;
   response.set_wallet_id(hdWallet->walletId());
   response.set_private_key(privKeyStr);
   response.set_chain_code(seedStr);
   return sendData(signer::GetDecryptedNodeType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onSetLimits(const std::string &data)
{
   signer::SetLimitsRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   bs::signer::Limits limits;
   limits.autoSignSpendXBT = request.auto_sign_satoshis();
   limits.manualSpendXBT = request.manual_satoshis();
   limits.autoSignTimeS = request.auto_sign_time();
   limits.manualPassKeepInMemS = request.password_keep_in_mem();
   app_->setLimits(limits);
   return true;
}

bool SignerAdapterListener::onSyncSettings(const std::string &data)
{
   signer::Settings settings;
   if (!settings.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->updateSettings(settings);
   return true;
}

bool SignerAdapterListener::onControlPasswordReceived(const std::string &data)
{
   signer::EnterControlPasswordRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->setControlPassword(SecureBinaryData::fromString(request.controlpassword()));
   return true;
}

bool SignerAdapterListener::onChangeControlPassword(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ChangeControlPasswordRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   bs::error::ErrorCode result = app_->changeControlPassword(SecureBinaryData::fromString(request.controlpasswordold())
      , SecureBinaryData::fromString(request.controlpasswordnew()));

   signer::ChangePasswordResponse response;
   response.set_errorcode(static_cast<uint32_t>(result));
   sendData(signer::ChangeControlPasswordType, response.SerializeAsString(), reqId);

   return true;
}

bool SignerAdapterListener::onWindowsStatus(const std::string &data, bs::signer::RequestId)
{
   headless::WindowStatus msg;
   if (!msg.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->windowVisibilityChanged(msg.visible());
   return true;
}

bool SignerAdapterListener::onVerifyOfflineTx(const std::string &data, bs::signer::RequestId reqId)
{
   signer::VerifyOfflineTxRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   signer::VerifyOfflineTxResponse response;

   auto parsedReqs = bs::core::wallet::ParseOfflineTXFile(request.content());
   if (parsedReqs.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "empty offline sign request");
      response.set_error_code(static_cast<uint32_t>(bs::error::ErrorCode::FailedToParse));
      return sendData(signer::VerifyOfflineTxRequestType, response.SerializeAsString(), reqId);
   }

   for (const auto &parsedReq : parsedReqs) {
      auto errorCode = verifyOfflineSignRequest(parsedReq);
      if (errorCode != bs::error::ErrorCode::NoError) {
         SPDLOG_LOGGER_ERROR(logger_, "offline sign request verification failed");
         response.set_error_code(static_cast<uint32_t>(errorCode));
         return sendData(signer::VerifyOfflineTxRequestType, response.SerializeAsString(), reqId);
      }
   }

   response.set_error_code(static_cast<uint32_t>(bs::error::ErrorCode::NoError));
   sendData(signer::VerifyOfflineTxRequestType, response.SerializeAsString(), reqId);
   return true;
}

bool SignerAdapterListener::onPasswordReceived(const std::string &data)
{
   signer::DecryptWalletEvent request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->passwordReceived(request.wallet_id(), static_cast<bs::error::ErrorCode>(request.errorcode())
      , BinaryData::fromString(request.password()));
   return true;
}

bool SignerAdapterListener::onRequestClose()
{
   logger_->info("[SignerAdapterListener::{}] closing on interface request", __func__);
   app_->close();
   return true;
}

bool SignerAdapterListener::onAutoSignRequest(const std::string &data, bs::signer::RequestId reqId)
{
   signer::AutoSignActRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }

   bs::error::ErrorCode result = app_->activateAutoSign(request.rootwalletid(), request.activateautosign()
      , SecureBinaryData::fromString(request.password()));

   signer::AutoSignActResponse response;
   response.set_errorcode(static_cast<uint32_t>(result));
   response.set_rootwalletid(request.rootwalletid());

   sendData(signer::AutoSignActType, response.SerializeAsString(), reqId);

   return true;
}

bool SignerAdapterListener::onChangePassword(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ChangePasswordResponse response;
   signer::ChangePasswordRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerContainerListener] failed to parse ChangePasswordRequest");
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::FailedToParse));
      response.set_root_wallet_id(std::string());
      sendData(signer::ChangePasswordType, response.SerializeAsString(), reqId);
      return false;
   }
   const auto &wallet = walletsMgr_->getHDWalletById(request.root_wallet_id());
   if (!wallet) {
      logger_->error("[SignerContainerListener] failed to find wallet for id {}", request.root_wallet_id());
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::WalletNotFound));
      response.set_root_wallet_id(request.root_wallet_id());
      sendData(signer::ChangePasswordType, response.SerializeAsString(), reqId);
      return false;
   }
   std::vector<bs::wallet::PasswordData> pwdData;
   for (int i = 0; i < request.new_password_size(); ++i) {
      const auto &pwd = request.new_password(i);
      pwdData.push_back({ SecureBinaryData::fromString(pwd.password())
         , { static_cast<bs::wallet::EncryptionType>(pwd.enctype()), BinaryData::fromString(pwd.enckey())} } );
   }

   if (!request.remove_old() && pwdData.empty()) {
      logger_->error("[SignerContainerListener] can't change/add empty password for {}", request.root_wallet_id());
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::MissingPassword));
      response.set_root_wallet_id(request.root_wallet_id());
      sendData(signer::ChangePasswordType, response.SerializeAsString(), reqId);
      return false;
   }
   if (!request.add_new() && (pwdData.size() > 1)) {
      logger_->error("[SignerContainerListener] can't remove/change more than 1 password at a time for {}"
         , request.root_wallet_id());
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::InternalError));
      response.set_root_wallet_id(request.root_wallet_id());
      sendData(signer::ChangePasswordType, response.SerializeAsString(), reqId);
      return false;
   }

   const bs::wallet::PasswordData oldPass = { SecureBinaryData::fromString(request.old_password().password())
   ,  {static_cast<bs::wallet::EncryptionType>(request.old_password().enctype())
      , BinaryData::fromString(request.old_password().enckey()) } };

   if (request.remove_old()) {
      logger_->warn("[SignerContainerListener] password removal is not supported, yet");
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::InternalError));
      response.set_root_wallet_id(request.root_wallet_id());
      sendData(signer::ChangePasswordType, response.SerializeAsString(), reqId);
      return false;
   }

   bool result = true;
   {
      const bs::core::WalletPasswordScoped lock(wallet, oldPass.password);
      if (request.add_new()) {
         for (const auto &pwd : pwdData) {
            result &= wallet->addPassword(pwd);
         }
      }
      else {
         result = wallet->changePassword(oldPass.metaData, pwdData.front());
      }
   }
   if (result) {
      walletsListUpdated();
   }

   if (result) {
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::NoError));
   }
   else {
      response.set_errorcode(static_cast<int>(bs::error::ErrorCode::InvalidPassword));
   }
   response.set_root_wallet_id(request.root_wallet_id());
   logger_->info("[SignerAdapterListener::{}] password changed for wallet {} with result {}"
      , __func__, request.root_wallet_id(), result);

   return sendData(signer::ChangePasswordType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onCreateHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::CreateHDWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerContainerListener] failed to parse CreateHDWalletRequest");
      return false;
   }

   bs::wallet::PasswordData pwdData;
   pwdData.password = SecureBinaryData::fromString(request.password().password());
   pwdData.metaData = { static_cast<bs::wallet::EncryptionType>(request.password().enctype())
      , BinaryData::fromString(request.password().enckey()) };
   pwdData.controlPassword = app_->controlPassword();

   try {
      const auto &w = request.wallet();
      const auto netType = w.testnet() ? NetworkType::TestNet : NetworkType::MainNet;
      const auto seed = w.privatekey().empty() ? bs::core::wallet::Seed(SecureBinaryData::fromString(w.seed()), netType)
         : bs::core::wallet::Seed::fromXpriv(SecureBinaryData::fromString(w.privatekey()), netType);

      if (walletsMgr_->getHDWalletById(seed.getWalletId()) != nullptr) {
         signer::CreateHDWalletResponse response;
         response.set_errorcode(static_cast<uint32_t>(bs::error::ErrorCode::WalletAlreadyPresent));
         return sendData(signer::CreateHDWalletType, response.SerializeAsString(), reqId);
      }

      const auto wallet = walletsMgr_->createWallet(w.name(), w.description(), seed
         , settings_->getWalletsDir(), pwdData, w.primary(), w.create_legacy_leaf());

      walletsListUpdated();

      signer::CreateHDWalletResponse response;
      response.set_wallet_id(wallet->walletId());
      response.set_errorcode(static_cast<uint32_t>(bs::error::ErrorCode::NoError));
      return sendData(signer::CreateHDWalletType, response.SerializeAsString(), reqId);
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to create HD Wallet: {}", __func__, e.what());
   }
   signer::CreateHDWalletResponse response;
   response.set_errorcode(static_cast<uint32_t>(bs::error::ErrorCode::InternalError));
   return sendData(signer::CreateHDWalletType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onDeleteHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   headless::DeleteHDWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   const auto &walletId = request.rootwalletid();
   const auto &wallet = walletsMgr_->getHDWalletById(walletId);
   if (!wallet) {
      logger_->error("[{}] failed to find HD Wallet by id {}", __func__, walletId);

      headless::DeleteHDWalletResponse response;
      response.set_error(fmt::format("Can't find wallet {}", request.rootwalletid()));
      return sendData(signer::DeleteHDWalletType, response.SerializeAsString(), reqId);
   }

   logger_->debug("Deleting HDWallet {}: {}", walletId, wallet->name());

   bool result = walletsMgr_->deleteWalletFile(wallet);
   if (result) {
      walletsListUpdated();
   }

   headless::DeleteHDWalletResponse response;
   response.set_success(result);
   if (!result) {
      response.set_error("Unknown error");
   }

   return sendData(signer::DeleteHDWalletType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onImportWoWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ImportWoWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   if (!SystemFileUtils::pathExist(settings_->getWalletsDir())) {
      if (SystemFileUtils::mkPath(settings_->getWalletsDir())) {
         logger_->info("[{}] created missing wallets dir {}", __func__, settings_->getWalletsDir());
      }
      else {
         logger_->error("[{}] failed to create wallets dir {}", __func__, settings_->getWalletsDir());
         return false;
      }
   }

   const std::string filePath = settings_->getWalletsDir() + "/" + request.filename();
   {
      std::ofstream ofs(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!ofs.good()) {
         logger_->error("[{}] failed to write to {}", __func__, filePath);
         return false;
      }
      ofs << request.content();
   }

   const auto woWallet = walletsMgr_->loadWoWallet(settings_->netType()
      , settings_->getWalletsDir(), request.filename(), app_->controlPassword());
   if (!woWallet) {
      SystemFileUtils::rmFile(filePath);
      return false;
   }
   walletsListUpdated();
   return sendWoWallet(woWallet, signer::ImportWoWalletType, reqId);
}

bool SignerAdapterListener::onImportHwWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ImportHwWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   bs::core::wallet::HwWalletInfo info{
      static_cast<bs::wallet::HardwareEncKey::WalletType>(request.wallettype()),
      request.vendor(),
      request.label(),
      request.deviceid(),
      request.xpubroot(),
      request.xpubnestedsegwit(),
      request.xpubnativesegwit(),
      request.xpublegacy()
   };

   const auto woWallet = walletsMgr_->createHwWallet(settings_->netType()
      , info, settings_->getWalletsDir(), app_->controlPassword());
   if (!woWallet) {
      return false;
   }
   walletsListUpdated();
   return sendWoWallet(woWallet, signer::ImportHwWalletType, reqId);
}

bool SignerAdapterListener::onExportWoWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ExportWoWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   auto woWallet = walletsMgr_->getHDWalletById(request.rootwalletid());
   if (!woWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find wallet with walletId: {}", request.rootwalletid());
      return false;
   }

   bool isForked = false;
   if (!woWallet->isWatchingOnly()) {
      woWallet = woWallet->createWatchingOnly();
      if (!woWallet) {
         SPDLOG_LOGGER_ERROR(logger_, "forking as WO wallet failed: {}", request.rootwalletid());
         return false;
      }
      isForked = true;
   }

   auto eraseFile = ScopedGuard([isForked, woWallet] {
      if (isForked) {
         // Remove forked WO file from wallets dir
         woWallet->eraseFile();
      }
   });

   std::ifstream file(woWallet->getFileName(), std::ios::in | std::ios::binary | std::ios::ate);
   if (!file.is_open()) {
      SPDLOG_LOGGER_ERROR(logger_, "can't open WO file for read: {}", woWallet->getFileName());
      return false;
   }

   auto size = file.tellg();
   std::string content;
   content.resize(size_t(size));

   if (size == 0) {
      SPDLOG_LOGGER_ERROR(logger_, "empty WO file: {}", woWallet->getFileName());
      return false;
   }

   file.seekg(0, std::ios::beg);
   file.read(&content.front(), size);

   signer::ExportWoWalletResponse response;
   response.set_content(std::move(content));
   return sendData(signer::ExportWoWalletType, response.SerializeAsString(), reqId);
}

void SignerAdapterListener::walletsListUpdated()
{
   logger_->debug("[{}]", __func__);
   app_->walletsListUpdated();
   sendData(signer::WalletsListUpdatedType, {});
}

void SignerAdapterListener::onStarted()
{
   started_ = true;
   sendReady();
}

void SignerAdapterListener::shutdownIfNeeded()
{
   if (settings_->runMode() == bs::signer::RunMode::litegui && app_) {
      logger_->info("terminal disconnect detected, shutdown...");
      app_->close();
   }
}

bool SignerAdapterListener::sendReady()
{
   if (!started_) {
      return true;
   }

   // Notify GUI about bind status
   sendStatusUpdate();

   return sendData(signer::HeadlessReadyType, {});
}

bs::error::ErrorCode SignerAdapterListener::verifyOfflineSignRequest(const bs::core::wallet::TXSignRequest &txSignReq)
{
   if (!txSignReq.allowBroadcasts && txSignReq.expiredTimestamp == std::chrono::system_clock::time_point{}) {
      SPDLOG_LOGGER_ERROR(logger_, "expiration timestamp must be set for offline settlement requests");
      return bs::error::ErrorCode::TxInvalidRequest;
   }
   if (txSignReq.expiredTimestamp != std::chrono::system_clock::time_point{}
       && txSignReq.expiredTimestamp < std::chrono::system_clock::now()) {
      SPDLOG_LOGGER_ERROR(logger_, "settlement have been expired already");
      return bs::error::ErrorCode::TxSettlementExpired;
   }

   if (txSignReq.walletIds.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "wallet(s) not specified");
      return bs::error::ErrorCode::WalletNotFound;
   }

   auto checkIndexValidity = [this](const std::string &index) {
      if (index.empty()) {
         SPDLOG_LOGGER_ERROR(logger_, "empty path found, must be set for offline signer");
         return false;
      }

      try {
         auto path = bs::hd::Path::fromString(index);

         if (path.length() != 2) {
            SPDLOG_LOGGER_ERROR(logger_, "path length must be 2");
            return false;
         }
         if (path.get(0) != bs::core::hd::Leaf::addrTypeExternal_
             && path.get(0) != bs::core::hd::Leaf::addrTypeInternal_) {
            SPDLOG_LOGGER_ERROR(logger_, "found unknown path at level 0: '{}', must be 0 or 1", path.get(0));
            return false;
         }
         if (path.get(1) >= bs::hd::hardFlag) {
            SPDLOG_LOGGER_ERROR(logger_, "found hardened path at level 1: '{}', must be non-hardened", path.get(1));
            return false;
         }
      } catch (const std::exception &e) {
         SPDLOG_LOGGER_ERROR(logger_, "parsing index failed: {}", e.what());
         return false;
      }

      return true;
   };

   auto hdWallet = walletsMgr_->getHDRootForLeaf(txSignReq.walletIds.front());
   if (!hdWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find HD wallet for leaf '{}'", txSignReq.walletIds.front());
      return bs::error::ErrorCode::WalletNotFound;
   }
   if (hdWallet->isWatchingOnly() && !hdWallet->isHardwareWallet()) {
      SPDLOG_LOGGER_ERROR(logger_, "can't sign with watching-only HD wallet {}", hdWallet->walletId());
      return bs::error::ErrorCode::WalletNotFound;
   }

   size_t foundInputCount = 0;
   auto checkAddress = [](const bs::core::WalletsManager::WalletPtr& wallet,
      bs::Address addr) {
      return wallet->defaultAddressType() == addr.getType() ||
         (addr.getType() == AddressEntryType_P2SH && (wallet->defaultAddressType() & addr.getType()));
   };

   for (const auto &walletId : txSignReq.walletIds) { // sync new addresses in all wallets
      const auto wallet = walletsMgr_->getWalletById(walletId);
      if (!wallet) {
         SPDLOG_LOGGER_ERROR(logger_, "failed to find wallet with id {}", walletId);
         return bs::error::ErrorCode::WalletNotFound;
      }
      if (walletsMgr_->getHDRootForLeaf(walletId) != hdWallet) {
         SPDLOG_LOGGER_ERROR(logger_, "different HD roots used");
         return bs::error::ErrorCode::WalletNotFound;
      }
      if (wallet->type() != bs::core::wallet::Type::Bitcoin) {
         SPDLOG_LOGGER_ERROR(logger_, "only XBT leaves supported");
         return bs::error::ErrorCode::WalletNotFound;
      }

      for (size_t i = 0; i < txSignReq.armorySigner_.getTxInCount(); ++i) {
         auto spender = txSignReq.armorySigner_.getSpender(i);
         const auto addr = bs::Address::fromScript(spender->getOutputScript());
         if (!checkAddress(wallet, addr)) {
            continue;
         }
         // Need to extend used address chain for offline wallets
         //TODO: fix me. wallet->synchronizeUsedAddressChain(txSignReq.inputIndices.at(i));
         const auto addrEntry = wallet->getAddressEntryForAddr(addr.id());
         if (!addrEntry) {
            SPDLOG_LOGGER_ERROR(logger_, "can't find input with address {} in wallet {}"
               , addr.display(), walletId);
            return bs::error::ErrorCode::WrongAddress;
         }
         foundInputCount += 1;
      }
   }
   if (txSignReq.armorySigner_.getTxInCount() != foundInputCount) {
      SPDLOG_LOGGER_ERROR(logger_, "failed to find all inputs");
      return bs::error::ErrorCode::WalletNotFound;
   }

   // Verify that change belongs to the same HD wallet
   if (txSignReq.change.value > 0) {
      if (!checkIndexValidity(txSignReq.change.index)) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid change address index");
         return bs::error::ErrorCode::FailedToParse;
      }

      // Need to extend change wallet too (find change wallet by change type).
      std::shared_ptr<bs::core::hd::Leaf> changeWallet;
      for (const auto &leaf : hdWallet->getLeaves()) {
         if (leaf->type() == bs::core::wallet::Type::Bitcoin
            && checkAddress(leaf, txSignReq.change.address)) {
            changeWallet = leaf;
            break;
         }
      }
      if (!changeWallet) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find change wallet");
         return bs::error::ErrorCode::WrongAddress;
      }
      changeWallet->synchronizeUsedAddressChain(txSignReq.change.index);

      // Verify that change address is valid
      if (txSignReq.change.index != changeWallet->getAddressIndex(txSignReq.change.address)) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid change address");
         return bs::error::ErrorCode::WrongAddress;
      }
   }

   return bs::error::ErrorCode::NoError;
}
