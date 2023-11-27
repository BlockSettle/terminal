/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MDHistAdapter.h"
#include <spdlog/spdlog.h>
#include "TerminalMessage.h"

#include "common.pb.h"

using namespace BlockSettle::Common;
using namespace bs::message;


MDHistAdapter::MDHistAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::MDHistory))
{}

ProcessingResult MDHistAdapter::process(const bs::message::Envelope &)
{
   return ProcessingResult::Ignored;
}

bool MDHistAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.foreignId());
         return false;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         AdministrativeMessage admMsg;
         admMsg.set_component_loading(user_->value());
         pushBroadcast(UserTerminal::create(TerminalUsers::System)
            , admMsg.SerializeAsString());
      }
   }
   return false;
}
