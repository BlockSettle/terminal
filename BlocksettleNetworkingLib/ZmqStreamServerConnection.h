/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ZMQ_STREAM_SERVER_CONNECTION_H__
#define __ZMQ_STREAM_SERVER_CONNECTION_H__

#include "ZmqServerConnection.h"

#include <unordered_map>
#include <atomic>

class ActiveStreamClient;

// it is called streamserver, cause stream sockets are used
// currently it is not general server and support only genoa protocol
// to change that - look at onZeroFrame and server_protocol_connection
class ZmqStreamServerConnection : public ZmqServerConnection
{
   friend class ActiveStreamClient;

protected:
   using server_connection = ActiveStreamClient;
   using server_connection_ptr = std::shared_ptr<server_connection>;

public:
   ZmqStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context);
   ~ZmqStreamServerConnection() noexcept override = default;

   ZmqStreamServerConnection(const ZmqStreamServerConnection&) = delete;
   ZmqStreamServerConnection& operator = (const ZmqStreamServerConnection&) = delete;

   ZmqStreamServerConnection(ZmqStreamServerConnection&&) = delete;
   ZmqStreamServerConnection& operator = (ZmqStreamServerConnection&&) = delete;

   bool SendDataToClient(const std::string& clientId, const std::string& data
      , const SendResultCb &cb = nullptr) override;
   bool SendDataToAllClients(const std::string& data, const SendResultCb &cb = nullptr) override;
protected:
   ZmqContext::sock_ptr CreateDataSocket() override;

   bool ReadFromDataSocket() override;

   bool sendRawData(const std::string& clientId, const std::string& rawData
      , const SendResultCb &cb = nullptr);

   virtual server_connection_ptr CreateActiveConnection() = 0;

private:
   void onZeroFrame(const std::string& clientId);
   void onDataFrameReceived(const std::string& clientId, const std::string& data);

   server_connection_ptr findConnection(const std::string& clientId);

private:
   std::atomic_flag                 connectionsLockFlag_ = ATOMIC_FLAG_INIT;

   std::unordered_map<std::string, server_connection_ptr> activeConnections_;
};

#endif // __ZMQ_STREAM_SERVER_CONNECTION_H__
