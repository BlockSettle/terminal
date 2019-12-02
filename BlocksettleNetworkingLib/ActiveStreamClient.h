/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ACTIVE_STREAM_SERVER_CONNECTION_H__
#define __ACTIVE_STREAM_SERVER_CONNECTION_H__

#include <string>
#include <memory>

namespace spdlog
{
   class logger;
}

class ZmqStreamServerConnection;

class ActiveStreamClient
{
public:
   ActiveStreamClient(const std::shared_ptr<spdlog::logger>& logger);
   virtual ~ActiveStreamClient() noexcept = default;

   ActiveStreamClient(const ActiveStreamClient&) = delete;
   ActiveStreamClient& operator = (const ActiveStreamClient&) = delete;

   ActiveStreamClient(ActiveStreamClient&&) = delete;
   ActiveStreamClient& operator = (ActiveStreamClient&&) = delete;

   void InitConnection(const std::string& connectionId, ZmqStreamServerConnection* serverConnection);

public:
   virtual bool send(const std::string& data) = 0;

   virtual void onRawDataReceived(const std::string& rawData) = 0;

protected:
   bool sendRawData(const std::string& data);
   void notifyOnData(const std::string& data);

protected:
   std::shared_ptr<spdlog::logger> logger_;

private:
   std::string connectionId_;
   ZmqStreamServerConnection* serverConnection_;
};

#endif // __ACTIVE_STREAM_SERVER_CONNECTION_H__