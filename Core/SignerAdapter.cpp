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
#include "bip39/bip39.h"
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

ProcessingResult SignerAdapter::process(const bs::message::Envelope &env)
{
   if (env.isRequest()) {
      SignerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own msg #{}", __func__, env.foreignId());
         return ProcessingResult::Error;
      }
      return processOwnRequest(env, msg);
   }
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings msg #{}", __func__, env.foreignId());
         return ProcessingResult::Error;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kSignerResponse:
         return processSignerSettings(msg.signer_response());
      case SettingsMessage::kNewKeyResponse:
         return processNewKeyResponse(msg.new_key_response());
      default: break;
      }
   }
   return ProcessingResult::Ignored;
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
   logger_->debug("[SignerAdapter::start]");
   SettingsMessage msg;
   msg.mutable_signer_request();
   pushRequest(user_, UserTerminal::create(TerminalUsers::Settings)
      , msg.SerializeAsString());
}

ProcessingResult SignerAdapter::processOwnRequest(const bs::message::Envelope &env
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
      return processCreateWallet(env, false, request.create_wallet());
   case SignerMessage::kImportWallet:
      return processCreateWallet(env, true, request.import_wallet());
   case SignerMessage::kDeleteWallet:
      return processDeleteWallet(env, request.delete_wallet());
   case SignerMessage::kImportHwWallet:
      return processImportHwWallet(env, request.import_hw_wallet());
   case SignerMessage::kExportWoWallet:
      return processExportWoWallet(env, request.export_wo_wallet());
   case SignerMessage::kChangeWalletPass:
      return processChangeWalletPass(env, request.change_wallet_pass());
   case SignerMessage::kGetWalletSeed:
      return processGetWalletSeed(env, request.get_wallet_seed());
   default:
      logger_->warn("[{}] unknown signer request: {}", __func__, request.data_case());
      break;
   }
   return ProcessingResult::Ignored;
}

ProcessingResult SignerAdapter::processSignerSettings(const SettingsMessage_SignerServer &response)
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
   sendComponentLoading();
   return ProcessingResult::Success;
}

void SignerAdapter::walletsChanged(bool rescan)
{
   SignerMessage msg;
   msg.set_wallets_list_updated(rescan);
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
   pushBroadcast(adminUser, msg.SerializeAsString());
   return true;
}

ProcessingResult SignerAdapter::processNewKeyResponse(bool acceptNewKey)
{
   if (!connFuture_) {
      logger_->error("[{}] new key comparison wasn't requested", __func__);
      return ProcessingResult::Error;
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processStartWalletSync(const bs::message::Envelope &env)
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSyncAddresses(const bs::message::Envelope &env
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSyncNewAddresses(const bs::message::Envelope &env
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
         return ProcessingResult::Error;
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processExtendAddrChain(const bs::message::Envelope &env
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSyncWallet(const bs::message::Envelope &env
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
      logger_->debug("[SignerAdapter::processSyncWallet] wallet {} asset type: {}", walletId, data.assetType);
      SignerMessage msg;
      auto msgResp = msg.mutable_wallet_synced();
      msgResp->set_wallet_id(walletId);
      msgResp->set_high_ext_index(data.highestExtIndex);
      msgResp->set_high_int_index(data.highestIntIndex);
      msgResp->set_asset_type((int)data.assetType);

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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSyncHdWallet(const bs::message::Envelope &env
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSyncAddrComment(const SignerMessage_SyncAddressComment &request)
{
   try {
      signer_->syncAddressComment(request.wallet_id()
         , bs::Address::fromAddressString(request.address()), request.comment());
   }
   catch (const std::exception &) {}
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSyncTxComment(const SignerMessage_SyncTxComment &request)
{
   signer_->syncTxComment(request.wallet_id()
      , BinaryData::fromString(request.tx_hash()), request.comment());
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processGetRootPubKey(const bs::message::Envelope &env
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processDelHdRoot(const std::string &walletId)
{
   signer_->DeleteHDRoot(walletId);
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processDelHdLeaf(const std::string &walletId)
{
   signer_->DeleteHDLeaf(walletId);
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processSignTx(const bs::message::Envelope& env
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
   bs::core::WalletsManager::HDWalletPtr hdWallet;
   if ((txReq.walletIds.size() == 1)) {
      hdWallet = walletsMgr_->getHDWalletById(txReq.walletIds.at(0));
      if (hdWallet) {
         if (!hdWallet->isHardwareWallet()) {
            hdWallet.reset();
         }
      }
      else {
         logger_->error("[{}] failed to get HD wallet by {}", __func__, txReq.walletIds.at(0));
      }
   }
   if (hdWallet) {
      const auto& signData = SecureBinaryData::fromString(request.passphrase());
      hdWallet->pushPasswordPrompt([signData]() { return signData; });
      SignerMessage msg;
      auto msgResp = msg.mutable_sign_tx_response();
      try {
         const auto& signedTX = hdWallet->signTXRequestWithWallet(txReq);
         msgResp->set_signed_tx(signedTX.toBinStr());
      }
      catch (const std::exception& e) {
         msgResp->set_error_text(e.what());
      }
      hdWallet->popPasswordPrompt();
      pushResponse(user_, env, msg.SerializeAsString());
   }
   else {
      passphrase_ = SecureBinaryData::fromString(request.passphrase());
      signer_->signTXRequest(txReq, cbSigned
         , static_cast<SignContainer::TXSignMode>(request.sign_mode())
         , request.keep_dup_recips());
   }
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processResolvePubSpenders(const bs::message::Envelope& env
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
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processAutoSignRequest(const bs::message::Envelope& env
   , const SignerMessage_AutoSign& request)
{
   autoSignRequests_[request.wallet_id()] = env;
   QVariantMap data;
   data[QLatin1String("rootId")] = QString::fromStdString(request.wallet_id());
   data[QLatin1String("enable")] = request.enable();
   signer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ActivateAutoSign, data);
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processDialogRequest(const bs::message::Envelope&
   , const SignerMessage_DialogRequest& request)
{
   const auto& dlgType = static_cast<bs::signer::ui::GeneralDialogType>(request.dialog_type());
   QVariantMap data;
   for (const auto& d : request.data()) {
      data[QString::fromStdString(d.key())] = QString::fromStdString(d.value());
   }
   signer_->customDialogRequest(dlgType, data);
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processCreateWallet(const bs::message::Envelope& env
   , bool rescan, const SignerMessage_CreateWalletRequest& w)
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
      walletsChanged(rescan);
      logger_->debug("[{}] wallet {} created", __func__, wallet->walletId());
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to create wallet: {}", __func__, e.what());
      msgResp->set_error_msg(e.what());
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

bs::message::ProcessingResult SignerAdapter::processImportHwWallet(const bs::message::Envelope& env
   , const BlockSettle::Common::SignerMessage_ImportHWWallet& request)
{
   logger_->debug("[{}] {}", __func__, request.DebugString());
   const bs::core::HwWalletInfo hwwInfo{ static_cast<bs::wallet::HardwareEncKey::WalletType>(request.type())
      , request.vendor(), request.label(), request.device_id(), request.xpub_root()
      , request.xpub_nested_segwit(), request.xpub_native_segwit(), request.xpub_legacy() };
   SignerMessage msg;
   auto msgResp = msg.mutable_created_wallet();
   try {
      logger_->debug("[{}] label: {}, vendor: {}", __func__, hwwInfo.label, hwwInfo.vendor);
      const auto& hwWallet = std::make_shared<bs::core::hd::Wallet>(netType_
         , hwwInfo, walletsDir_, logger_);
      walletsMgr_->addWallet(hwWallet);
      msgResp->set_wallet_id(hwWallet->walletId());
      walletsChanged(true);
      logger_->debug("[{}] wallet {} created", __func__, hwWallet->walletId());
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to create HW wallet: {}", __func__, e.what());
      msgResp->set_error_msg(e.what());
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return bs::message::ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processDeleteWallet(const bs::message::Envelope& env
   , const SignerMessage_WalletRequest& request)
{
   SignerMessage msg;
   const auto& hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (hdWallet && walletsMgr_->deleteWalletFile(hdWallet)) {
      msg.set_wallet_deleted(request.wallet_id());
   }
   else {
      msg.set_wallet_deleted("");
   }
   pushBroadcast(user_, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult SignerAdapter::processExportWoWallet(const bs::message::Envelope& env
   , const SignerMessage_WalletRequest& request)
{
   const auto& hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (!hdWallet) {
      logger_->error("[{}] wallet {} not found", __func__, request.wallet_id());
      return ProcessingResult::Error;
   }
   if (hdWallet->isWatchingOnly()) {
      logger_->error("[{}] can't export {} (already watching only)", __func__, request.wallet_id());
      return ProcessingResult::Error;
   }
   auto woWallet = hdWallet->createWatchingOnly();
   if (!woWallet) {
      logger_->error("[{}] WO export {} failed", __func__, request.wallet_id());
      return ProcessingResult::Error;
   }
   logger_->debug("[{}] exported {} to {}", __func__, request.wallet_id(), woWallet->getFileName());
   return ProcessingResult::Success;
}

bs::message::ProcessingResult SignerAdapter::processChangeWalletPass(const bs::message::Envelope& env
   , const SignerMessage_ChangeWalletPassword& request)
{
   const auto& hdWallet = walletsMgr_->getHDWalletById(request.wallet().wallet_id());
   if (!hdWallet) {
      logger_->error("[{}] wallet {} not found", __func__, request.wallet().wallet_id());
      return ProcessingResult::Error;
   }
   bool result = true;
   {
      const auto oldPass = SecureBinaryData::fromString(request.wallet().password());
      const bs::wallet::PasswordData newPass{ SecureBinaryData::fromString(request.new_password())
         , {bs::wallet::EncryptionType::Password} };
      const bs::core::WalletPasswordScoped lock(hdWallet, oldPass);
      result = hdWallet->changePassword({bs::wallet::EncryptionType::Password}, newPass);
   }
   SignerMessage msg;
   msg.set_wallet_pass_changed(result);
   pushResponse(user_, env, msg.SerializeAsString());
   return bs::message::ProcessingResult::Success;
}

bs::message::ProcessingResult SignerAdapter::processGetWalletSeed(const bs::message::Envelope& env
   , const SignerMessage_WalletRequest& request)
{
   SignerMessage msg;
   auto msgResp = msg.mutable_wallet_seed();
   msgResp->set_wallet_id(request.wallet_id());
   const auto& hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (!hdWallet) {
      logger_->error("[{}] wallet {} not found", __func__, request.wallet_id());
      pushResponse(user_, env, msg.SerializeAsString());
      return ProcessingResult::Error;
   }
   try {
      const bs::core::WalletPasswordScoped lock(hdWallet, SecureBinaryData::fromString(request.password()));
      const auto& seed = hdWallet->getDecryptedSeed();
      msgResp->set_xpriv(seed.toXpriv().toBinStr());
      msgResp->set_seed(seed.seed().toBinStr());

      if (seed.hasPrivateKey()) {
         const auto& privKey = seed.privateKey();
         std::vector<uint8_t> privData;
         for (int i = 0; i < (int)privKey.getSize(); ++i) {
            privData.push_back(privKey.getPtr()[i]);
         }
         const auto& words = BIP39::create_mnemonic(privData);
         std::string bip39Seed;
         for (const auto& w : words) {
            bip39Seed += w + " ";
         }
         bip39Seed.pop_back();
         msgResp->set_bip39_seed(bip39Seed);
      }
   }
   catch (const Armory::Wallets::WalletException& e) {
      logger_->error("[{}] failed to decrypt wallet {}: {}", __func__, request.wallet_id(), e.what());
      pushResponse(user_, env, msg.SerializeAsString());
      return ProcessingResult::Error;
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return bs::message::ProcessingResult::Success;
}
