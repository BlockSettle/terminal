/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AssetsAdapter.h"
#include <spdlog/spdlog.h>
#include "TerminalMessage.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;


AssetsAdapter::AssetsAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Assets))
{}

bool AssetsAdapter::process(const bs::message::Envelope &env)
{
   return true;
}
