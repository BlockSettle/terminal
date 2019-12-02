/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __DATA_CONNECTION_LISTENER_H__
#define __DATA_CONNECTION_LISTENER_H__

#include <string>

class DataConnectionListener
{
public:
   enum DataConnectionError
   {
      NoError,
      UndefinedSocketError,
      HostNotFoundError,
      HandshakeFailed,
      SerializationFailed,
      HeartbeatWaitFailed,
      ConnectionTimeout,
   };

public:
   DataConnectionListener() = default;
   virtual ~DataConnectionListener() noexcept = default;

   DataConnectionListener(const DataConnectionListener&) = delete;
   DataConnectionListener& operator = (const DataConnectionListener&) = delete;

   DataConnectionListener(DataConnectionListener&&) = delete;
   DataConnectionListener& operator = (DataConnectionListener&&) = delete;

public:
   virtual void OnDataReceived(const std::string& data) = 0;
   virtual void OnConnected() = 0;
   virtual void OnDisconnected() = 0;
   virtual void OnError(DataConnectionError errorCode) = 0;
};

#endif // __DATA_CONNECTION_LISTENER_H__
