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
#include "ConnectionManager.h"
#include "SslCaBundle.h"
#include "TerminalMessage.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;


BsServerAdapter::BsServerAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::BsServer))
{
   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   connMgr_->setCaBundle(bs::caBundlePtr(), bs::caBundleSize());
}

bool BsServerAdapter::process(const bs::message::Envelope &env)
{
   return true;
}
