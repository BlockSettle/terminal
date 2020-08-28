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
   else if (env.receiver && (env.receiver->value<TerminalUsers>() == TerminalUsers::BsServer)) {
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

bool BsServerAdapter::processPuBKeyResponse(bool allowed)
{
   if (!futPuBkey_) {
      logger_->error("[{}] not waiting for PuB key permission", __func__);
      return true;
   }
   futPuBkey_->setValue(allowed);
   futPuBkey_.reset();
}
