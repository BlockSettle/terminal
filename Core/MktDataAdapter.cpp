/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MktDataAdapter.h"
#include <spdlog/spdlog.h>
#include "BSMarketDataProvider.h"
#include "ConnectionManager.h"
#include "TerminalMessage.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;
using namespace bs::message;


MktDataAdapter::MktDataAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::MktData))
{
   auto connMgr = std::make_shared<ConnectionManager>(logger_);
   mdProvider_ = std::make_shared<BSMarketDataProvider>(connMgr, logger_, this
      , true, false);
}

bool MktDataAdapter::process(const bs::message::Envelope &env)
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

void MktDataAdapter::userWantsToConnect()
{

}

void MktDataAdapter::waitingForConnectionDetails()
{
   //TODO: request remote settings
}
