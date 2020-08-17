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
#include "TerminalMessage.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;


MktDataAdapter::MktDataAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::MktData))
{}

bool MktDataAdapter::process(const bs::message::Envelope &env)
{
   return true;
}
