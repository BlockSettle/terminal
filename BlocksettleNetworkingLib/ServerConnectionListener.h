/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SERVER_CONNECTION_LISTENER_H__
#define __SERVER_CONNECTION_LISTENER_H__

#include <memory>
#include <string>

class ServerConnectionListener
{
public:
   enum ClientError
   {
      NoError = 0,

      // Reported when client do not have valid credentials (unknown public key)
      HandshakeFailed = 1,
   };

   ServerConnectionListener() = default;
   virtual ~ServerConnectionListener() noexcept = default;

   ServerConnectionListener(const ServerConnectionListener&) = delete;
   ServerConnectionListener& operator = (const ServerConnectionListener&) = delete;

   ServerConnectionListener(ServerConnectionListener&&) = delete;
   ServerConnectionListener& operator = (ServerConnectionListener&&) = delete;

public:
   virtual void OnDataFromClient(const std::string& clientId, const std::string& data) = 0;

   virtual void OnClientConnected(const std::string& clientId) = 0;
   virtual void OnClientDisconnected(const std::string& clientId) = 0;

   virtual void OnPeerConnected(const std::string &) {}
   virtual void OnPeerDisconnected(const std::string &) {}

   virtual void onClientError(const std::string &clientId, const std::string &error) {}
   virtual void onClientError(const std::string &clientId, ClientError errorCode, int socket) {}
};

#endif // __SERVER_CONNECTION_LISTENER_H__
