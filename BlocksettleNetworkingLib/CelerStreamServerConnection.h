/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_STREAM_SERVER_CONNECTION_H__
#define __CELER_STREAM_SERVER_CONNECTION_H__

#include "ZmqStreamServerConnection.h"

class CelerStreamServerConnection : public ZmqStreamServerConnection
{
public:
   CelerStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context);
   ~CelerStreamServerConnection() noexcept = default;

   CelerStreamServerConnection(const CelerStreamServerConnection&) = delete;
   CelerStreamServerConnection& operator = (const CelerStreamServerConnection&) = delete;

   CelerStreamServerConnection(CelerStreamServerConnection&&) = delete;
   CelerStreamServerConnection& operator = (CelerStreamServerConnection&&) = delete;

protected:
   server_connection_ptr CreateActiveConnection() override;
};

#endif // __CELER_STREAM_SERVER_CONNECTION_H__