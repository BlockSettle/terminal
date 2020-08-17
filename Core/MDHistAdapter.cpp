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

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;


MDHistAdapter::MDHistAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::MDHistory))
{}

bool MDHistAdapter::process(const bs::message::Envelope &env)
{
   return true;
}
