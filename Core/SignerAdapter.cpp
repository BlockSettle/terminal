/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include "Adapters/SignerClient.h"
#include "ConnectionManager.h"
#include "HeadlessContainer.h"
#include "ProtobufHeadlessUtils.h"
#include "TerminalMessage.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Signer))
{}

std::unique_ptr<SignerClient> SignerAdapter::createClient() const
{
   auto client = std::make_unique<SignerClient>(logger_, user_);
   client->setQueue(queue_);
   return client;
}

bool SignerAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->value<TerminalUsers>() == TerminalUsers::System) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case AdministrativeMessage::kStart:
      case AdministrativeMessage::kRestart:
         start();
         break;
      default: break;
      }
   }
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kSignerResponse:
         return processSignerSettings(msg.signer_response());
      }
   }
   else if (env.receiver && (env.receiver->value<TerminalUsers>() == TerminalUsers::Signer)) {
      SignerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own msg #{}", __func__, env.id);
         return true;
      }
      if (env.request) {
         return processOwnRequest(env, msg);
      }
      else {
         switch (msg.data_case()) {
         case SignerMessage::kNewKeyResponse:
            return processNewKeyResponse(msg.new_key_response());
         default: break;
         }
      }
   }
   return true;
}

void SignerAdapter::start()
{
   SettingsMessage msg;
   msg.mutable_signer_request();
   bs::message::Envelope env{ 0, user_, UserTerminal::create(TerminalUsers::Settings)
      , {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

bool SignerAdapter::processOwnRequest(const bs::message::Envelope &env
   , const SignerMessage &request)
{
   switch (request.data_case()) {
   case SignerMessage::kStartWalletsSync:
      return processStartWalletSync(env);
   case SignerMessage::kSyncAddresses:
      return processSyncAddresses(env, request.sync_addresses());
   case SignerMessage::kSyncNewAddresses:
      return processSyncNewAddresses(env, request.sync_new_addresses());
   case SignerMessage::kExtAddrChain:
      return processExtendAddrChain(env, request.ext_addr_chain());
   case SignerMessage::kSyncWallet:
      return processSyncWallet(env, request.sync_wallet());
   case SignerMessage::kSyncHdWallet:
      return processSyncHdWallet(env, request.sync_hd_wallet());
   case SignerMessage::kSyncAddrComment:
      return processSyncAddrComment(request.sync_addr_comment());
   case SignerMessage::kSyncTxComment:
      return processSyncTxComment(request.sync_tx_comment());
   case SignerMessage::kSetSettlId:
      return processSetSettlId(env, request.set_settl_id());
   case SignerMessage::kGetRootPubkey:
      return processGetRootPubKey(env, request.get_root_pubkey());
   case SignerMessage::kDelHdRoot:
      return processDelHdRoot(request.del_hd_root());
   case SignerMessage::kDelHdLeaf:
      return processDelHdLeaf(request.del_hd_leaf());
   case SignerMessage::kSignTxRequest:
      return processSignTx(env, request.sign_tx_request());
   case SignerMessage::kSetUserId:
      return processSetUserId(request.set_user_id().user_id()
         , request.set_user_id().wallet_id());
   case SignerMessage::kCreateSettlWallet:
      return processCreateSettlWallet(env, request.create_settl_wallet());
   case SignerMessage::kGetSettlPayinAddr:
      return processGetPayinAddress(env, request.get_settl_payin_addr());
   case SignerMessage::kResolvePubSpenders:
      return processResolvePubSpenders(env
         , bs::signer::pbTxRequestToCore(request.resolve_pub_spenders()));
   case SignerMessage::kSignSettlementTx:
      return processSignSettlementTx(env, request.sign_settlement_tx());
   case SignerMessage::kAutoSign:
      return processAutoSignRequest(env, request.auto_sign());
   default:
      logger_->warn("[{}] unknown signer request: {}", __func__, request.data_case());
      break;
   }
   return true;
}

std::shared_ptr<WalletSignerContainer> SignerAdapter::makeRemoteSigner(
   const BlockSettle::Terminal::SettingsMessage_SignerServer &response)
{
   const auto &netType = static_cast<NetworkType>(response.network_type());
   const auto &connMgr = std::make_shared<ConnectionManager>(logger_);
   const auto &cbOurNewKey = [this](const std::string &oldKey, const std::string &newKey
      , const std::string &srvAddrPort
      , const std::shared_ptr<FutureValue<bool>> &newKeyProm)
   {
      connFuture_ = newKeyProm;
      connKey_ = newKey;

      SignerMessage msg;
      auto msgReq = msg.mutable_new_key_request();
      msgReq->set_old_key(oldKey);
      msgReq->set_new_key(newKey);
      msgReq->set_server_id(srvAddrPort);
      bs::message::Envelope env{ 0, user_, nullptr, {}, {}
         , msg.SerializeAsString(), true };
      pushFill(env);
   };

   auto remoteSigner = std::make_shared<RemoteSigner>(logger_
      , QString::fromStdString(response.host()), QString::fromStdString(response.port())
      , netType, connMgr, this, SignContainer::OpMode::Remote, false
      , response.remote_keys_dir(), response.remote_keys_file(), cbOurNewKey);

   bs::network::BIP15xPeers peers;
   for (const auto &clientKey : response.client_keys()) {
      try {
         const BinaryData signerKey = BinaryData::CreateFromHex(clientKey.value());
         peers.push_back(bs::network::BIP15xPeer(clientKey.key(), signerKey));
      } catch (const std::exception &e) {
         logger_->warn("[{}] invalid signer key: {}", __func__, e.what());
      }
   }
   remoteSigner->updatePeerKeys(peers);
   return remoteSigner;
}

bool SignerAdapter::processSignerSettings(const SettingsMessage_SignerServer &response)
{
   curServerId_ = response.id();
   if (response.is_local()) {
      QLatin1String localSignerHost("127.0.0.1");
      QString localSignerPort;
      const auto &netType = static_cast<NetworkType>(response.network_type());

      for (int attempts = 0; attempts < 10; ++attempts) {
         // https://tools.ietf.org/html/rfc6335
         // the Dynamic Ports, also known as the Private or Ephemeral Ports,
         // from 49152-65535 (never assigned)
         auto port = 49152 + rand() % 16000;

         auto portToTest = QString::number(port);

         if (!SignerConnectionExists(localSignerHost, portToTest)) {
            localSignerPort = portToTest;
            break;
         } else {
            logger_->debug("[SignerAdapter::processSignerSettings] attempt #{}:"
               " port {} used", port);
         }
      }

      if (localSignerPort.isEmpty()) {
         logger_->error("[SignerAdapter::processSignerSettings] failed to find not busy port");
         SignerMessage msg;
         auto msgError = msg.mutable_state();
         msgError->set_code((int)SignContainer::SocketFailed);
         msgError->set_text("failed to bind local port");
         Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
         return pushFill(env);
      }

      const auto &connMgr = std::make_shared<ConnectionManager>(logger_);
      const bool startLocalSignerProcess = true;
      signer_ = std::make_shared<LocalSigner>(logger_
         , QString::fromStdString(response.home_dir()), netType, localSignerPort
         , connMgr, this, startLocalSignerProcess, "", ""
         , response.auto_sign_spend_limit());
      signer_->Start();
      return sendComponentLoading();
   }
   else {
      signer_ = makeRemoteSigner(response);
      signer_->Start();
      return sendComponentLoading();
   }
   return true;
}


void SignerAdapter::connError(SignContainer::ConnectionError errCode, const QString &errMsg)
{
   SignerMessage msg;
   auto msgErr = msg.mutable_state();
   msgErr->set_code((int)errCode);
   msgErr->set_text(errMsg.toStdString());
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::connTorn()
{
   SignerMessage msg;
   auto msgState = msg.mutable_state();
   msgState->set_code((int)SignContainer::ConnectionError::SignerGoesOffline);
   msgState->set_text("disconnected");
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::authLeafAdded(const std::string &walletId)
{
   SignerMessage msg;
   msg.set_auth_leaf_added(walletId);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::walletsChanged()
{
   logger_->debug("[{}]", __func__);
   SignerMessage msg;
   msg.mutable_wallets_list_updated();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::onReady()
{
   SignerMessage msg;
   auto msgState = msg.mutable_state();
   msgState->set_code((int)SignContainer::Ready);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::walletsReady()
{
   SignerMessage msg;
   msg.mutable_wallets_ready_to_sync();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::newWalletPrompt()
{
   logger_->debug("[{}]", __func__);
   SignerMessage msg;
   msg.mutable_need_new_wallet_prompt();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void SignerAdapter::autoSignStateChanged(bs::error::ErrorCode code
   , const std::string& walletId)
{
   const auto& itAS = autoSignRequests_.find(walletId);
   if (itAS == autoSignRequests_.end()) {
      logger_->warn("[{}] no request found for {}", __func__, walletId);
      return;
   }
   bool enabled = false;
   if (code == bs::error::ErrorCode::NoError) {
      enabled = true;
   }
   else if (code == bs::error::ErrorCode::AutoSignDisabled) {
      enabled = false;
   }
   else {
      logger_->error("[{}] auto sign {} error code: {}", __func__, walletId
         , (int)code);
   }
   SignerMessage msg;
   auto msgResp = msg.mutable_auto_sign();
   msgResp->set_wallet_id(walletId);
   msgResp->set_enable(enabled);
   Envelope envResp{ itAS->second.id, user_, itAS->second.sender, {}, {}
      , msg.SerializeAsString() };
   pushFill(envResp);
   autoSignRequests_.erase(itAS);
}

void SignerAdapter::windowIsVisible(bool flag)
{
   SignerMessage msg;
   msg.set_window_visible_changed(flag);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

bool SignerAdapter::sendComponentLoading()
{
   static const auto &adminUser = UserTerminal::create(TerminalUsers::System);
   AdministrativeMessage msg;
   msg.set_component_loading(user_->value());
   Envelope env{ 0, adminUser, nullptr, {}, {}, msg.SerializeAsString() };
   return pushFill(env);
}

bool SignerAdapter::processNewKeyResponse(bool acceptNewKey)
{
   if (!connFuture_) {
      logger_->error("[{}] new key comparison wasn't requested", __func__);
      return true;
   }
   connFuture_->setValue(acceptNewKey);
   if (acceptNewKey) {
      SettingsMessage msg;
      auto msgReq = msg.mutable_signer_set_key();
      msgReq->set_server_id(curServerId_);
      msgReq->set_new_key(connKey_);
      bs::message::Envelope env{ 0, user_, UserTerminal::create(TerminalUsers::Settings)
         , {}, {}, msg.SerializeAsString(), true };
      pushFill(env);
   }
   connFuture_.reset();
   return true;
}

bool SignerAdapter::processStartWalletSync(const bs::message::Envelope &env)
{
   requests_[env.id] = env.sender;
   const auto &cbWallets = [this, msgId=env.id]
      (const std::vector<bs::sync::WalletInfo> &wi)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_wallets_info();
      for (const auto &entry : wi) {
         auto wallet = msgResp->add_wallets();
         wallet->set_format((int)entry.format);
         for (const auto &id : entry.ids) {
            wallet->add_ids(id);
         }
         wallet->set_name(entry.name);
         wallet->set_description(entry.description);
         wallet->set_network_type((int)entry.netType);
         wallet->set_watch_only(entry.watchOnly);
         for (const auto &encType : entry.encryptionTypes) {
            wallet->add_encryption_types((int)encType);
         }
         for (const auto &encKey : entry.encryptionKeys) {
            wallet->add_encryption_keys(encKey.toBinStr());
         }
         auto keyRank = wallet->mutable_encryption_rank();
         keyRank->set_m(entry.encryptionRank.m);
         keyRank->set_n(entry.encryptionRank.n);
      }
      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   signer_->syncWalletInfo(cbWallets);
   return true;
}

bool SignerAdapter::processSyncAddresses(const bs::message::Envelope &env
   , const SignerMessage_SyncAddresses &request)
{
   requests_[env.id] = env.sender;
   const auto &cb = [this, msgId = env.id, walletId = request.wallet_id()]
      (bs::sync::SyncState st)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_sync_addr_result();
      msgResp->set_wallet_id(walletId);
      msgResp->set_status(static_cast<int>(st));

      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   std::set<BinaryData> addrSet;
   for (const auto &addr : request.addresses()) {
      try {
         const auto &address = bs::Address::fromAddressString(addr);
         addrSet.insert(address.prefixed());
      }
      catch (const std::exception &) {}
   }
   signer_->syncAddressBatch(request.wallet_id(), addrSet, cb);
   return true;
}

bool SignerAdapter::processSyncNewAddresses(const bs::message::Envelope &env
   , const SignerMessage_SyncNewAddresses &request)
{
   requests_[env.id] = env.sender;
   if (request.single()) {
      const auto &cb = [this, msgId = env.id, walletId = request.wallet_id()]
      (const bs::Address &addr)
      {
         const auto &itReq = requests_.find(msgId);
         if (itReq == requests_.end()) {
            return;
         }
         SignerMessage msg;
         auto msgResp = msg.mutable_new_addresses();
         msgResp->set_wallet_id(walletId);
         msgResp->add_addresses()->set_address(addr.display());

         Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
         pushFill(envResp);
         requests_.erase(itReq);
      };
      if (request.indices_size() != 1) {
         logger_->error("[{}] not a single new address request", __func__);
         return true;
      }
      signer_->syncNewAddress(request.wallet_id(), request.indices(0), cb);
   }
   else {
      const auto &cb = [this, msgId=env.id, walletId = request.wallet_id()]
         (const std::vector<std::pair<bs::Address, std::string>> &addrIdxPairs)
      {
         const auto &itReq = requests_.find(msgId);
         if (itReq == requests_.end()) {
            return;
         }
         SignerMessage msg;
         auto msgResp = msg.mutable_new_addresses();
         msgResp->set_wallet_id(walletId);
         for (const auto &aiPair : addrIdxPairs) {
            auto msgPair = msgResp->add_addresses();
            msgPair->set_address(aiPair.first.display());
            msgPair->set_index(aiPair.second);
         }

         Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
         pushFill(envResp);
         requests_.erase(itReq);
      };
      std::vector<std::string> indices;
      indices.reserve(request.indices_size());
      for (const auto &idx : request.indices()) {
         indices.push_back(idx);
      }
      signer_->syncNewAddresses(request.wallet_id(), indices, cb);
   }
   return true;
}

bool SignerAdapter::processExtendAddrChain(const bs::message::Envelope &env
   , const SignerMessage_ExtendAddrChain &request)
{
   requests_[env.id] = env.sender;
   const auto &cb = [this, msgId = env.id, walletId = request.wallet_id()]
      (const std::vector<std::pair<bs::Address, std::string>> &addrIdxPairs)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_addr_chain_extended();
      msgResp->set_wallet_id(walletId);
      for (const auto &aiPair : addrIdxPairs) {
         auto msgPair = msgResp->add_addresses();
         msgPair->set_address(aiPair.first.display());
         msgPair->set_index(aiPair.second);
      }

      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   signer_->extendAddressChain(request.wallet_id(), request.count(), request.ext_int(), cb);
   return true;
}

bool SignerAdapter::processSyncWallet(const bs::message::Envelope &env
   , const std::string &walletId)
{
   requests_[env.id] = env.sender;
   const auto &cb = [this, msgId=env.id, walletId]
      (bs::sync::WalletData data)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_wallet_synced();
      msgResp->set_wallet_id(walletId);
      msgResp->set_high_ext_index(data.highestExtIndex);
      msgResp->set_high_int_index(data.highestIntIndex);

      for (const auto &addr : data.addresses) {
         auto msgAddr = msgResp->add_addresses();
         msgAddr->set_index(addr.index);
         msgAddr->set_address(addr.address.display());
         msgAddr->set_comment(addr.comment);
      }
      for (const auto &addr : data.addrPool) {
         auto msgAddr = msgResp->add_addr_pool();
         msgAddr->set_index(addr.index);
         msgAddr->set_address(addr.address.display());
         msgAddr->set_comment(addr.comment);
      }
      for (const auto &txCom : data.txComments) {
         auto msgTxCom = msgResp->add_tx_comments();
         msgTxCom->set_tx_hash(txCom.txHash.toBinStr());
         msgTxCom->set_comment(txCom.comment);
      }

      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   signer_->syncWallet(walletId, cb);
   return true;
}

bool SignerAdapter::processSyncHdWallet(const bs::message::Envelope &env
   , const std::string &walletId)
{
   requests_[env.id] = env.sender;
   const auto &cb = [this, msgId = env.id, walletId]
      (bs::sync::HDWalletData data)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_hd_wallet_synced();
      *msgResp = data.toCommonMessage();
      msgResp->set_wallet_id(walletId);

      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   signer_->syncHDWallet(walletId, cb);
   return true;
}

bool SignerAdapter::processSyncAddrComment(const SignerMessage_SyncAddressComment &request)
{
   try {
      signer_->syncAddressComment(request.wallet_id()
         , bs::Address::fromAddressString(request.address()), request.comment());
   }
   catch (const std::exception &) {}
   return true;
}

bool SignerAdapter::processSyncTxComment(const SignerMessage_SyncTxComment &request)
{
   signer_->syncTxComment(request.wallet_id()
      , BinaryData::fromString(request.tx_hash()), request.comment());
   return true;
}

bool SignerAdapter::processSetSettlId(const bs::message::Envelope &env
   , const SignerMessage_SetSettlementId &request)
{
   requests_[env.id] = env.sender;
   const auto &cb = [this, msgId=env.id](bool result)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      msg.set_settl_id_set(result);
      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   signer_->setSettlementID(request.wallet_id()
      , BinaryData::fromString(request.settlement_id()), cb);
   return true;
}

bool SignerAdapter::processSignSettlementTx(const bs::message::Envelope& env
   , const SignerMessage_SignSettlementTx& request)
{
   const auto& signCb = [this, env, settlementId=request.settlement_id()]
      (bs::error::ErrorCode result, const BinaryData& signedTx)
   {
      SignerMessage msg;
      auto msgResp = msg.mutable_sign_tx_response();
      msgResp->set_id(settlementId);
      msgResp->set_error_code((int)result);
      msgResp->set_signed_tx(signedTx.toBinStr());
      Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
   };

   auto txReq = bs::signer::pbTxRequestToCore(request.tx_request(), logger_);
   const bs::sync::PasswordDialogData dlgData(request.details());
   if (request.contra_auth_pubkey().empty()) {
      signer_->signSettlementTXRequest(txReq, dlgData
         , SignContainer::TXSignMode::Full, false, signCb);
   }
   else {
      txReq.txHash = txReq.txId();
      const bs::core::wallet::SettlementData sd{
         BinaryData::fromString(request.settlement_id()),
         BinaryData::fromString(request.contra_auth_pubkey()), request.own_key_first() };
      signer_->signSettlementPayoutTXRequest(txReq, sd, dlgData, signCb);
   }
   return true;
}

bool SignerAdapter::processGetRootPubKey(const bs::message::Envelope &env
   , const std::string &walletId)
{
   requests_[env.id] = env.sender;
   const auto &cb = [this, msgId=env.id, walletId]
      (bool result, const SecureBinaryData &key)
   {
      const auto &itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_root_pubkey();
      msgResp->set_wallet_id(walletId);
      msgResp->set_pub_key(key.toBinStr());
      msgResp->set_success(result);

      Envelope envResp{ msgId, user_, itReq->second, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
      requests_.erase(itReq);
   };
   signer_->getRootPubkey(walletId, cb);
   return true;
}

bool SignerAdapter::processDelHdRoot(const std::string &walletId)
{
   return (signer_->DeleteHDRoot(walletId) > 0);
}

bool SignerAdapter::processDelHdLeaf(const std::string &walletId)
{
   return (signer_->DeleteHDLeaf(walletId) > 0);
}

bool SignerAdapter::processSignTx(const bs::message::Envelope& env
   , const SignerMessage_SignTxRequest& request)
{
   const auto& cbSigned = [this, env, id=request.id()]
      (BinaryData signedTX, bs::error::ErrorCode result, const std::string& errorReason)
   {
      SignerMessage msg;
      auto msgResp = msg.mutable_sign_tx_response();
      msgResp->set_id(id);
      msgResp->set_signed_tx(signedTX.toBinStr());
      msgResp->set_error_code((int)result);
      msgResp->set_error_text(errorReason);
      Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
   };
   const auto& txReq = bs::signer::pbTxRequestToCore(request.tx_request(), logger_);
   signer_->signTXRequest(txReq, cbSigned
      , static_cast<SignContainer::TXSignMode>(request.sign_mode())
      , request.keep_dup_recips());
   return true;
}

bool SignerAdapter::processSetUserId(const std::string& userId, const std::string& walletId)
{
   return (signer_->setUserId(BinaryData::fromString(userId), walletId) != 0);
}

bool SignerAdapter::processCreateSettlWallet(const bs::message::Envelope& env
   , const std::string& addrStr)
{
   bs::Address authAddr;
   try {
      authAddr = bs::Address::fromAddressString(addrStr);
   }
   catch (const std::exception& e) {
      logger_->error("[{}] invalid auth address {}: {}", __func__, addrStr, e.what());
      return true;
   }
   const auto& cb = [this, env](const SecureBinaryData& authPubKey)
   {
      SignerMessage msg;
      msg.set_auth_pubkey(authPubKey.toBinStr());
      Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
   };
   signer_->createSettlementWallet(authAddr, cb);
   return true;
}

bool SignerAdapter::processGetPayinAddress(const bs::message::Envelope& env
   , const SignerMessage_GetSettlPayinAddr& request)
{
   const auto& cbAddr = [this, env](bool success, const bs::Address& settlAddr)
   {
      SignerMessage msg;
      auto msgResp = msg.mutable_payin_address();
      msgResp->set_success(success);
      msgResp->set_address(settlAddr.display());
      Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
   };
   bs::core::wallet::SettlementData settlData{ BinaryData::fromString(request.settlement_id())
      , BinaryData::fromString(request.contra_auth_pubkey()), request.own_key_first() };
   signer_->getSettlementPayinAddress(request.wallet_id(), settlData, cbAddr);
   return true;
}

bool SignerAdapter::processResolvePubSpenders(const bs::message::Envelope& env
   , const bs::core::wallet::TXSignRequest& txReq)
{
   const auto& cbResolve = [this, env](bs::error::ErrorCode result
      , const Codec_SignerState::SignerState& state)
   {
      SignerMessage msg;
      auto msgResp = msg.mutable_resolved_spenders();
      msgResp->set_result((int)result);
      msgResp->set_signer_state(state.SerializeAsString());
      Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
   };
   if (signer_->resolvePublicSpenders(txReq, cbResolve) == 0) {
      logger_->error("[{}] failed to send", __func__);
   }
   return true;
}

bool SignerAdapter::processAutoSignRequest(const bs::message::Envelope& env
   , const SignerMessage_AutoSign& request)
{
   autoSignRequests_[request.wallet_id()] = env;
   QVariantMap data;
   data[QLatin1String("rootId")] = QString::fromStdString(request.wallet_id());
   data[QLatin1String("enable")] = request.enable();
   return (signer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ActivateAutoSign
      , data) != 0);
}
