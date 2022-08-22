/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include "Adapters/SignerClient.h"
#include "CoreWalletsManager.h"
#include "Wallets/InprocSigner.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "TerminalMessage.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<WalletSignerContainer>& signer)
   : logger_(logger), signer_(signer)
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
   if (env.isRequest()) {
      SignerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own msg #{}", __func__, env.foreignId());
         return true;
      }
      return processOwnRequest(env, msg);
   }
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings msg #{}", __func__, env.foreignId());
         return true;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kSignerResponse:
         return processSignerSettings(msg.signer_response());
      case SettingsMessage::kNewKeyResponse:
         return processNewKeyResponse(msg.new_key_response());
      default: break;
      }
   }
   return true;
}

bool SignerAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->value<TerminalUsers>() == TerminalUsers::System) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative msg #{}", __func__, env.foreignId());
         return true;
      }
      switch (msg.data_case()) {
      case AdministrativeMessage::kStart:
      case AdministrativeMessage::kRestart:
         start();
         return true;
      default: break;
      }
   }
   return false;
}

void SignerAdapter::start()
{
   if (signer_) {
      sendComponentLoading();
      onReady();
      walletsReady();
      return;
   }
   SettingsMessage msg;
   msg.mutable_signer_request();
   pushRequest(user_, UserTerminal::create(TerminalUsers::Settings)
      , msg.SerializeAsString());
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
   case SignerMessage::kGetRootPubkey:
      return processGetRootPubKey(env, request.get_root_pubkey());
   case SignerMessage::kDelHdRoot:
      return processDelHdRoot(request.del_hd_root());
   case SignerMessage::kDelHdLeaf:
      return processDelHdLeaf(request.del_hd_leaf());
   case SignerMessage::kSignTxRequest:
      return processSignTx(env, request.sign_tx_request());
   case SignerMessage::kResolvePubSpenders:
      return processResolvePubSpenders(env
         , bs::signer::pbTxRequestToCore(request.resolve_pub_spenders()));
   case SignerMessage::kAutoSign:
      return processAutoSignRequest(env, request.auto_sign());
   case SignerMessage::kDialogRequest:
      return processDialogRequest(env, request.dialog_request());
   case SignerMessage::kCreateWallet:
      return processCreateWallet(env, request.create_wallet());
   case SignerMessage::kImportWallet:
      return processCreateWallet(env, request.import_wallet());
   default:
      logger_->warn("[{}] unknown signer request: {}", __func__, request.data_case());
      break;
   }
   return true;
}

bool SignerAdapter::processSignerSettings(const SettingsMessage_SignerServer &response)
{
   curServerId_ = response.id();
   netType_ = static_cast<NetworkType>(response.network_type());
   walletsDir_ = response.home_dir();
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger_);
   signer_ = std::make_shared<InprocSigner>(walletsMgr_, logger_, this
      , walletsDir_, netType_, [this](const std::string& walletId)
         -> std::unique_ptr<bs::core::WalletPasswordScoped> {
      const auto& hdWallet = walletsMgr_->getHDWalletById(walletId);
      if (!hdWallet) {
         return nullptr;
      }
      return std::make_unique<bs::core::WalletPasswordScoped>(hdWallet, passphrase_);
   });
   logger_->info("[{}] loading wallets from {}", __func__, walletsDir_);
   signer_->Start();
   walletsChanged();
   return sendComponentLoading();
}

void SignerAdapter::walletsChanged()
{
   SignerMessage msg;
   msg.mutable_wallets_list_updated();
   pushBroadcast(user_, msg.SerializeAsString(), true);
}

void SignerAdapter::onReady()
{
   SignerMessage msg;
   auto msgState = msg.mutable_state();
   msgState->set_code((int)SignContainer::Ready);
   pushBroadcast(user_, msg.SerializeAsString(), true);
}

void SignerAdapter::walletsReady()
{
   SignerMessage msg;
   msg.mutable_wallets_ready_to_sync();
   pushBroadcast(user_, msg.SerializeAsString(), true);
}

void SignerAdapter::newWalletPrompt()
{
   logger_->debug("[{}]", __func__);
   SignerMessage msg;
   msg.mutable_need_new_wallet_prompt();
   pushBroadcast(user_, msg.SerializeAsString(), true);
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
   pushResponse(user_, itAS->second, msg.SerializeAsString());
   autoSignRequests_.erase(itAS);
}

bool SignerAdapter::sendComponentLoading()
{
   static const auto &adminUser = UserTerminal::create(TerminalUsers::System);
   AdministrativeMessage msg;
   msg.set_component_loading(user_->value());
   return pushBroadcast(adminUser, msg.SerializeAsString());
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
      pushRequest(user_, UserTerminal::create(TerminalUsers::Settings)
         , msg.SerializeAsString());
   }
   connFuture_.reset();
   return true;
}

bool SignerAdapter::processStartWalletSync(const bs::message::Envelope &env)
{
   requests_.put(env.foreignId(), env.sender);
   const auto &cbWallets = [this, msgId=env.foreignId()]
      (const std::vector<bs::sync::WalletInfo> &wi)
   {
      auto sender = requests_.take(msgId);
      if (!sender) {
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
      pushResponse(user_, sender, msg.SerializeAsString(), msgId);
   };
   signer_->syncWalletInfo(cbWallets);
   return true;
}

bool SignerAdapter::processSyncAddresses(const bs::message::Envelope &env
   , const SignerMessage_SyncAddresses &request)
{
   requests_.put(env.foreignId(), env.sender);
   const auto &cb = [this, msgId = env.foreignId(), walletId = request.wallet_id()]
      (bs::sync::SyncState st)
   {
      auto sender = requests_.take(msgId);
      if (!sender) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_sync_addr_result();
      msgResp->set_wallet_id(walletId);
      msgResp->set_status(static_cast<int>(st));

      pushResponse(user_, sender, msg.SerializeAsString(), msgId);
   };
   std::set<BinaryData> addrSet;
   for (const auto &addr : request.addresses()) {
      addrSet.insert(BinaryData::fromString(addr));
   }
   signer_->syncAddressBatch(request.wallet_id(), addrSet, cb);
   return true;
}

bool SignerAdapter::processSyncNewAddresses(const bs::message::Envelope &env
   , const SignerMessage_SyncNewAddresses &request)
{
   requests_.put(env.foreignId(), env.sender);
   if (request.single()) {
      const auto &cb = [this, msgId = env.foreignId(), walletId = request.wallet_id()]
         (const bs::Address &addr)
      {
         auto sender = requests_.take(msgId);
         if (!sender) {
            return;
         }
         SignerMessage msg;
         auto msgResp = msg.mutable_new_addresses();
         msgResp->set_wallet_id(walletId);
         msgResp->add_addresses()->set_address(addr.display());

         pushResponse(user_, sender, msg.SerializeAsString(), msgId);
      };
      if (request.indices_size() != 1) {
         logger_->error("[{}] not a single new address request", __func__);
         return true;
      }
      signer_->syncNewAddress(request.wallet_id(), request.indices(0), cb);
   }
   else {
      const auto &cb = [this, msgId=env.foreignId(), walletId = request.wallet_id()]
         (const std::vector<std::pair<bs::Address, std::string>> &addrIdxPairs)
      {
         auto sender = requests_.take(msgId);
         if (!sender) {
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
         pushResponse(user_, sender, msg.SerializeAsString(), msgId);
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
   requests_.put(env.foreignId(), env.sender);
   const auto &cb = [this, msgId = env.foreignId(), walletId = request.wallet_id()]
      (const std::vector<std::pair<bs::Address, std::string>> &addrIdxPairs)
   {
      auto sender = requests_.take(msgId);
      if (!sender) {
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
      pushResponse(user_, sender, msg.SerializeAsString(), msgId);
   };
   signer_->extendAddressChain(request.wallet_id(), request.count(), request.ext_int(), cb);
   return true;
}

bool SignerAdapter::processSyncWallet(const bs::message::Envelope &env
   , const std::string &walletId)
{
   requests_.put(env.foreignId(), env.sender);
   const auto &cb = [this, msgId=env.foreignId(), walletId]
      (bs::sync::WalletData data)
   {
      auto sender = requests_.take(msgId);
      if (!sender) {
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
      pushResponse(user_, sender, msg.SerializeAsString(), msgId);
   };
   signer_->syncWallet(walletId, cb);
   return true;
}

bool SignerAdapter::processSyncHdWallet(const bs::message::Envelope &env
   , const std::string &walletId)
{
   requests_.put(env.foreignId(), env.sender);
   const auto &cb = [this, msgId = env.foreignId(), walletId]
      (bs::sync::HDWalletData data)
   {
      auto sender = requests_.take(msgId);
      if (!sender) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_hd_wallet_synced();
      *msgResp = data.toCommonMessage();
      msgResp->set_wallet_id(walletId);

      pushResponse(user_, sender, msg.SerializeAsString(), msgId);
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

bool SignerAdapter::processGetRootPubKey(const bs::message::Envelope &env
   , const std::string &walletId)
{
   requests_.put(env.foreignId(), env.sender);
   const auto &cb = [this, msgId=env.foreignId(), walletId]
      (bool result, const SecureBinaryData &key)
   {
      auto sender = requests_.take(msgId);
      if (!sender) {
         return;
      }
      SignerMessage msg;
      auto msgResp = msg.mutable_root_pubkey();
      msgResp->set_wallet_id(walletId);
      msgResp->set_pub_key(key.toBinStr());
      msgResp->set_success(result);

      pushResponse(user_, sender, msg.SerializeAsString(), msgId);
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
      passphrase_.clear();
      SignerMessage msg;
      auto msgResp = msg.mutable_sign_tx_response();
      msgResp->set_id(id);
      msgResp->set_signed_tx(signedTX.toBinStr());
      msgResp->set_error_code((int)result);
      msgResp->set_error_text(errorReason);
      pushResponse(user_, env, msg.SerializeAsString());
   };
   const auto& txReq = bs::signer::pbTxRequestToCore(request.tx_request(), logger_);
   passphrase_ = SecureBinaryData::fromString(request.passphrase());
   signer_->signTXRequest(txReq, cbSigned
      , static_cast<SignContainer::TXSignMode>(request.sign_mode())
      , request.keep_dup_recips());
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
      pushResponse(user_, env, msg.SerializeAsString());
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

bool SignerAdapter::processDialogRequest(const bs::message::Envelope&
   , const SignerMessage_DialogRequest& request)
{
   const auto& dlgType = static_cast<bs::signer::ui::GeneralDialogType>(request.dialog_type());
   QVariantMap data;
   for (const auto& d : request.data()) {
      data[QString::fromStdString(d.key())] = QString::fromStdString(d.value());
   }
   return (signer_->customDialogRequest(dlgType, data) != 0);
}

bool SignerAdapter::processCreateWallet(const bs::message::Envelope& env
   , const SignerMessage_CreateWalletRequest& w)
{
   bs::wallet::PasswordData pwdData;
   pwdData.password = SecureBinaryData::fromString(w.password());
   pwdData.metaData = { bs::wallet::EncryptionType::Password };
   //FIXME: pwdData.controlPassword = controlPassword();

   SignerMessage msg;
   auto msgResp = msg.mutable_created_wallet();
   try {
      const auto& seed = w.xpriv_key().empty() ? bs::core::wallet::Seed(SecureBinaryData::fromString(w.seed()), netType_)
         : bs::core::wallet::Seed::fromXpriv(SecureBinaryData::fromString(w.xpriv_key()), netType_);

      const auto& wallet = walletsMgr_->createWallet(w.name(), w.description(), seed
         , walletsDir_, pwdData, w.primary());
      msgResp->set_wallet_id(wallet->walletId());
      walletsChanged();
      logger_->debug("[{}] wallet {} created", __func__, wallet->walletId());
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to create wallet: {}", __func__, e.what());
      msgResp->set_error_msg(e.what());
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return true;
}
