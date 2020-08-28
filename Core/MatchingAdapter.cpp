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
   : QObject(nullptr), logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Matching))
{
   celerConnection_ = std::make_shared<CelerClientProxy>(logger);
   connect(celerConnection_.get(), &BaseCelerClient::OnConnectedToServer, this
      , &MatchingAdapter::onCelerConnected, Qt::QueuedConnection);
   connect(celerConnection_.get(), &BaseCelerClient::OnConnectionClosed, this
      , &MatchingAdapter::onCelerDisconnected, Qt::QueuedConnection);
   connect(celerConnection_.get(), &BaseCelerClient::OnConnectionError, this
      , &MatchingAdapter::onCelerConnectionError, Qt::QueuedConnection);
}

void MatchingAdapter::onCelerConnected()
{
   MatchingMessage msg;
   auto loggedIn = msg.mutable_logged_in();
   loggedIn->set_user_type(static_cast<int>(celerConnection_->celerUserType()));
   loggedIn->set_user_id(celerConnection_->userId());
   loggedIn->set_user_name(celerConnection_->userName());

   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void MatchingAdapter::onCelerDisconnected()
{
   celerConnection_->CloseConnection();

   MatchingMessage msg;
   msg.mutable_logged_out();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void MatchingAdapter::onCelerConnectionError(int errorCode)
{
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
   return true;
}
