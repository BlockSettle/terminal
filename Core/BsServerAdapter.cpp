/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BsServerAdapter.h"
#include <spdlog/spdlog.h>
#include "Bip15xDataConnection.h"
#include "ConnectionManager.h"
#include "PubKeyLoader.h"
#include "RequestReplyCommand.h"
#include "SslCaBundle.h"
#include "TerminalMessage.h"
#include "WsDataConnection.h"

#include "bs_communication.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Terminal;
using namespace bs::message;


BsServerAdapter::BsServerAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::BsServer))
   , pubPort_(PubKeyLoader::serverHttpPort())
{
   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   connMgr_->setCaBundle(bs::caBundlePtr(), bs::caBundleSize());
}

bool BsServerAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case AdministrativeMessage::kStart:
         start();
         break;
      case AdministrativeMessage::kRestart:
         pubHost_.clear();
         hasNetworkSettings_ = false;
         start();
         break;
      }
   }
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings message #{}", __func__, env.id);
         return true;
      }
      if (msg.data_case() == SettingsMessage::kGetResponse) {
         return processLocalSettings(msg.get_response());
      }
   }
   else if (env.receiver->value<TerminalUsers>() == TerminalUsers::BsServer) {
      return processOwnRequest(env);
   }
   return true;
}

void BsServerAdapter::start()
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   auto req = msgReq->add_requests();
   req->set_source(SettingSource_Local);
   req->set_type(SettingType_Int);
   req->set_index(SetIdx_Environment);
   Envelope envReq{ 0, user_, UserTerminal::create(TerminalUsers::Settings)
      , {}, {}, msg.SerializeAsString() };
   pushFill(envReq);
}

bool BsServerAdapter::processOwnRequest(const Envelope &env)
{
   BsServerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse own request #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case BsServerMessage::kNetworkSettingsRequest:
      return processNetworkSettings(env);
   case BsServerMessage::kPubNewKeyResponse:
      return processPuBKeyResponse(msg.pub_new_key_response());
   default:    break;
   }
   return true;
}

bool BsServerAdapter::processLocalSettings(const SettingsMessage_SettingsResponse &response)
{
   for (const auto &setting : response.responses()) {
      switch (static_cast<ApplicationSettings::Setting>(setting.request().index())) {
      case ApplicationSettings::envConfiguration: {
         const auto &env = static_cast<ApplicationSettings::EnvConfiguration>(setting.i());
         pubHost_ = PubKeyLoader::serverHostName(PubKeyLoader::KeyType::PublicBridge, env);

         AdministrativeMessage admMsg;
         admMsg.set_component_loading(user_->value());
         Envelope envBC{ 0, UserTerminal::create(TerminalUsers::System), nullptr
            , {}, {}, admMsg.SerializeAsString() };
         pushFill(envBC);
      }
         break;

      default: break;
      }
   }
   return true;
}

bool BsServerAdapter::processNetworkSettings(const Envelope &env)
{
   if (pubHost_.empty()) {
      logger_->warn("[{}] no PuB host available", __func__);
      return false;
   }
   if (hasNetworkSettings_) {
      sendNetworkSettings(env);
      return true;
   }
   if (cmdNetworkSettings_) {
      return false;  // pool until settings request is complete
   }

   const auto &cbApprove = [this](const std::string& oldKey
      , const std::string& newKeyHex, const std::string& srvAddrPort
      , const std::shared_ptr<FutureValue<bool>> &newKeyProm)
   {
      futPuBkey_ = newKeyProm;
      BsServerMessage msg;
      auto msgReq = msg.mutable_pub_new_key_request();
      msgReq->set_old_key(oldKey);
      msgReq->set_new_key(newKeyHex);
      msgReq->set_server_id(srvAddrPort);
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString(), true };
      pushFill(env);
   };
   bs::network::BIP15xParams params;
   params.ephemeralPeers = true;
   const auto &bip15xTransport = std::make_shared<bs::network::TransportBIP15xClient>(logger_, params);
   bip15xTransport->setKeyCb(cbApprove);
   auto wsConn = std::make_unique<WsDataConnection>(logger_, WsDataConnectionParams{});
   auto connection = std::make_shared<Bip15xDataConnection>(logger_, std::move(wsConn), bip15xTransport);

   Blocksettle::Communication::RequestPacket reqPkt;
   reqPkt.set_requesttype(Blocksettle::Communication::GetNetworkSettingsType);
   reqPkt.set_requestdata("");

   cmdNetworkSettings_ = std::make_shared<RequestReplyCommand>("network_settings", connection, logger_);
   cmdNetworkSettings_->SetReplyCallback([this, env](const std::string &data) -> bool
   {
      if (data.empty()) {
         logger_->error("[BsServerAdapter::processNetworkSettings] empty reply from server");
         return false;
      }

      cmdNetworkSettings_->resetConnection();

      Blocksettle::Communication::GetNetworkSettingsResponse response;
      if (!response.ParseFromString(data)) {
         logger_->error("[BsServerAdapter::processNetworkSettings] invalid "
            "reply from BlockSettle server");
         return false;
      }
      if (!response.has_marketdata()) {
         logger_->error("[BsServerAdapter::processNetworkSettings] missing MD"
            " connection settings");
         return false;
      }
      if (!response.has_mdhs()) {
         logger_->error("[BsServerAdapter::processNetworkSettings] missing MDHS"
            " connection settings");
         return false;
      }
      if (!response.has_chat()) {
         logger_->error("[BsServerAdapter::processNetworkSettings] missing Chat"
            " connection settings");
         return false;
      }

      networkSettings_.marketData = { response.marketdata().host(), int(response.marketdata().port()) };
      networkSettings_.mdhs = { response.mdhs().host(), int(response.mdhs().port()) };
      networkSettings_.chat = { response.chat().host(), int(response.chat().port()) };
      networkSettings_.proxy = { response.proxy().host(), int(response.proxy().port()) };
      networkSettings_.status = response.status();
      networkSettings_.statusMsg = response.statusmsg();
      networkSettings_.isSet = true;

      cmdNetworkSettings_.reset();
      hasNetworkSettings_ = true;
      sendNetworkSettings(env);
      return true;
   });
}

void BsServerAdapter::sendNetworkSettings(const Envelope &env)
{
   if (!hasNetworkSettings_) {
      logger_->error("[{}] no network settings available atm", __func__);
   }
   BsServerMessage msg;
   auto msgResp = msg.mutable_network_settings_response();
   auto hostPort = msgResp->mutable_mkt_data();
   hostPort->set_host(networkSettings_.marketData.host);
   hostPort->set_port(std::to_string(networkSettings_.marketData.port));
   hostPort = msgResp->mutable_mdhs();
   hostPort->set_host(networkSettings_.mdhs.host);
   hostPort->set_port(std::to_string(networkSettings_.mdhs.port));
   hostPort = msgResp->mutable_chat();
   hostPort->set_host(networkSettings_.chat.host);
   hostPort->set_port(std::to_string(networkSettings_.chat.port));
   hostPort = msgResp->mutable_proxy();
   hostPort->set_host(networkSettings_.proxy.host);
   hostPort->set_port(std::to_string(networkSettings_.proxy.port));
   msgResp->set_status(networkSettings_.status);
   msgResp->set_status_message(networkSettings_.statusMsg);

   Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
   pushFill(envResp);
}

bool BsServerAdapter::processPuBKeyResponse(bool allowed)
{
   if (!futPuBkey_) {
      logger_->error("[{}] not waiting for PuB key permission", __func__);
      return true;
   }
   futPuBkey_->setValue(allowed);
   futPuBkey_.reset();
}
