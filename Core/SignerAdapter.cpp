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
#include "ConnectionManager.h"
#include "TerminalMessage.h"
#include "HeadlessContainer.h"
#include "Adapters/SignerClient.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), logger_(logger)
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
      , netType, connMgr, SignContainer::OpMode::Remote, false
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
      const auto &localSignerPort = QString::fromStdString(response.local_port());
      const auto &netType = static_cast<NetworkType>(response.network_type());

      if (SignerConnectionExists(localSignerHost, localSignerPort)) {
         logger_->error("[{}] failed to bind on local port {}", __func__, response.local_port());
         SignerMessage msg;
         auto msgError = msg.mutable_error();
         msgError->set_code((int)bs::error::ErrorCode::InternalError);
         msgError->set_text("failed to bind local port");
         Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
         return pushFill(env);
/*         BSMessageBox mbox(BSMessageBox::Type::question
            , tr("Local Signer Connection")
            , tr("Continue with Remote connection in Local GUI mode?")
            , tr("The Terminal failed to spawn the headless signer as the program is already running. "
               "Would you like to continue with remote connection in Local GUI mode?")
            , this);
         if (mbox.exec() == QDialog::Rejected) {
            return nullptr;
         }

         // Use locally started signer as remote
         signersProvider_->switchToLocalFullGUI(localSignerHost, localSignerPort);
         return createRemoteSigner(true);*/
      }

      const auto &connMgr = std::make_shared<ConnectionManager>(logger_);
      const bool startLocalSignerProcess = true;
      signer_ = std::make_shared<LocalSigner>(logger_
         , QString::fromStdString(response.home_dir()), netType, localSignerPort
         , connMgr, startLocalSignerProcess, "", "", response.auto_sign_spend_limit());
      connectSignals();
      signer_->Start();
      return sendComponentLoading();
   }
   else {
      signer_ = makeRemoteSigner(response);
      connectSignals();
      signer_->Start();
      return sendComponentLoading();
   }
   return true;
}

void SignerAdapter::connectSignals()
{
   if (!signer_) {
      return;
   }
   connect(signer_.get(), &SignContainer::connectionError, this, [this]
      (SignContainer::ConnectionError code, QString text) {
      SignerMessage msg;
      auto msgErr = msg.mutable_error();
      msgErr->set_code((int)code);
      msgErr->set_text(text.toStdString());
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &SignContainer::disconnected, this, [this] {
      SignerMessage msg;
      auto msgErr = msg.mutable_error();
      msgErr->set_code((int)SignContainer::ConnectionError::SignerGoesOffline);
      msgErr->set_text("disconnected");
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::AuthLeafAdded, this, [this](const std::string &leafId) {
      SignerMessage msg;
      msg.set_auth_leaf_added(leafId);
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::walletsListUpdated, this, [this] {
      SignerMessage msg;
      msg.mutable_wallets_list_updated();
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::walletsStorageDecrypted, this, [this] {
      SignerMessage msg;
      msg.mutable_wallet_storage_decrypted();
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::ready, this, [this] {
      SignerMessage msg;
      msg.mutable_ready();
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::needNewWalletPrompt, this, [this] {
      SignerMessage msg;
      msg.mutable_need_new_wallet_prompt();
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::walletsReadyToSync, this, [this] {
      SignerMessage msg;
      msg.mutable_wallets_ready_to_sync();
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);

   connect(signer_.get(), &WalletSignerContainer::windowVisibilityChanged, this, [this](bool visible) {
      SignerMessage msg;
      msg.set_window_visible_changed(visible);
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   }, Qt::QueuedConnection);
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
         wallet->set_id(entry.id);
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
