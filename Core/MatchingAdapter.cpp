/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MatchingAdapter.h"
#include <spdlog/spdlog.h>
#include "CelerClientProxy.h"
#include "TerminalMessage.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;
using namespace bs::message;


MatchingAdapter::MatchingAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Matching))
{
   celerConnection_ = std::make_unique<ClientCelerConnection>(logger, this, true, true);
}

void MatchingAdapter::connectedToServer()
{
   logger_->debug("[{}]", __func__);
   MatchingMessage msg;
   auto loggedIn = msg.mutable_logged_in();
   loggedIn->set_user_type(static_cast<int>(celerConnection_->celerUserType()));
   loggedIn->set_user_id(celerConnection_->userId());
   loggedIn->set_user_name(celerConnection_->userName());

   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void MatchingAdapter::connectionClosed()
{
   celerConnection_->CloseConnection();
   logger_->debug("[{}]", __func__);

   MatchingMessage msg;
   msg.mutable_logged_out();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void MatchingAdapter::connectionError(int errorCode)
{
   logger_->debug("[{}] {}", __func__, errorCode);
   MatchingMessage msg;
   msg.set_connection_error(errorCode);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}


bool MatchingAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.id);
         return true;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         AdministrativeMessage admMsg;
         admMsg.set_component_loading(user_->value());
         Envelope envBC{ 0, UserTerminal::create(TerminalUsers::System), nullptr
            , {}, {}, admMsg.SerializeAsString() };
         pushFill(envBC);
      }
   }
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::BsServer) {
      BsServerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse BsServer message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case BsServerMessage::kRecvMatching:
         celerConnection_->recvData(static_cast<CelerAPI::CelerMessageType>(msg.recv_matching().message_type())
            , msg.recv_matching().data());
         break;
      default: break;
      }
   }
   else if (env.receiver && (env.receiver->value() == user_->value())) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kLogin:
         return processLogin(msg.login());
      case MatchingMessage::kLogout:
         if (celerConnection_ && celerConnection_->IsConnected()) {
            celerConnection_->CloseConnection();
         }
         break;
      case MatchingMessage::kGetSubmittedAuthAddresses:
         return processGetSubmittedAuth(env);
      case MatchingMessage::kSubmitAuthAddress:
         return processSubmitAuth(env, msg.submit_auth_address());
      default:
         logger_->warn("[{}] unknown msg {} #{} from {}", __func__, msg.data_case()
            , env.id, env.sender->name());
         break;
      }
   }
   return true;
}

bool MatchingAdapter::processLogin(const MatchingMessage_Login& request)
{
   return celerConnection_->SendLogin(request.matching_login(), request.terminal_login(), {});
}

bool MatchingAdapter::processGetSubmittedAuth(const bs::message::Envelope& env)
{
   MatchingMessage msg;
   auto msgResp = msg.mutable_submitted_auth_addresses();
   for (const auto& addr : celerConnection_->GetSubmittedAuthAddressSet()) {
      msgResp->add_addresses(addr);
   }
   Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
   return pushFill(envResp);
}

bool MatchingAdapter::processSubmitAuth(const bs::message::Envelope& env
   , const std::string& address)
{
   auto submittedAddresses = celerConnection_->GetSubmittedAuthAddressSet();
   submittedAddresses.insert(address);
   celerConnection_->SetSubmittedAuthAddressSet(submittedAddresses);
   return processGetSubmittedAuth(env);
}


ClientCelerConnection::ClientCelerConnection(const std::shared_ptr<spdlog::logger>& logger
   , MatchingAdapter* parent, bool userIdRequired, bool useRecvTimer)
   : BaseCelerClient(logger, parent, userIdRequired, useRecvTimer)
   , parent_(parent)
{}

void ClientCelerConnection::onSendData(CelerAPI::CelerMessageType messageType
   , const std::string& data)
{
   BsServerMessage msg;
   auto msgReq = msg.mutable_send_matching();
   msgReq->set_message_type((int)messageType);
   msgReq->set_data(data);
   Envelope env{ 0, parent_->user_, bs::message::UserTerminal::create(bs::message::TerminalUsers::BsServer)
      , {}, {}, msg.SerializeAsString(), true };
   parent_->pushFill(env);
}
