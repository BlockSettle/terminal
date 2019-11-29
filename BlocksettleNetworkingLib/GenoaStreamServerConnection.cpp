/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "GenoaStreamServerConnection.h"

#include "GenoaConnection.h"
#include "ActiveStreamClient.h"

GenoaStreamServerConnection::GenoaStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
 : ZmqStreamServerConnection(logger, context)
{}

ZmqStreamServerConnection::server_connection_ptr GenoaStreamServerConnection::CreateActiveConnection()
{
   return std::make_shared<GenoaConnection<ActiveStreamClient>>(logger_);
}