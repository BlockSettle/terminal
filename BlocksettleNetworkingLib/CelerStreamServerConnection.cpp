/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerStreamServerConnection.h"

#include "CelerClientConnection.h"
#include "ActiveStreamClient.h"

CelerStreamServerConnection::CelerStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
 : ZmqStreamServerConnection(logger, context)
{}

ZmqStreamServerConnection::server_connection_ptr CelerStreamServerConnection::CreateActiveConnection()
{
   return std::make_shared<CelerClientConnection<ActiveStreamClient>>(logger_);
}