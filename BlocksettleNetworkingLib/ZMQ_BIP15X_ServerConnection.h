#ifndef __ZMQ_BIP15X_SERVERCONNECTION_H__
#define __ZMQ_BIP15X_SERVERCONNECTION_H__

#include <spdlog/spdlog.h>
//#include <string>
//#include <memory>
#include "AuthorizedPeers.h"
#include "BIP150_151.h"
#include "ZmqServerConnection.h"
#include "ZMQ_BIP15X_Msg.h"

#define SERVER_AUTH_PEER_FILENAME "server.peers"

#include <QString>
#include "ZmqServerConnection.h"
#include "EncryptionUtils.h"

class ZMQ_BIP15X_ServerConnection : public ZmqServerConnection
{
public:
   ZMQ_BIP15X_ServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , const QStringList& trustedClients, const uint64_t& id
      , const bool& ephemeralPeers);

   ~ZMQ_BIP15X_ServerConnection() noexcept override = default;

   ZMQ_BIP15X_ServerConnection(const ZMQ_BIP15X_ServerConnection&) = delete;
   ZMQ_BIP15X_ServerConnection& operator= (const ZMQ_BIP15X_ServerConnection&) = delete;

   ZMQ_BIP15X_ServerConnection(ZMQ_BIP15X_ServerConnection&&) = delete;
   ZMQ_BIP15X_ServerConnection& operator= (ZMQ_BIP15X_ServerConnection&&) = delete;

   bool SendDataToClient(const std::string& clientId, const std::string& data
      , const SendResultCb &cb = nullptr) override;

protected:
   ZmqContext::sock_ptr CreateDataSocket() override;
   bool ConfigDataSocket(const ZmqContext::sock_ptr& dataSocket) override;
   bool ReadFromDataSocket() override;

private:
   void ProcessIncomingData(const std::string& encData
      , const std::string& clientID);
   bool processAEADHandshake(const BinaryData& msgObj
      , const std::string& clientID);
   void promptUser(const BinaryDataRef& newKey, const std::string& srvAddrPort);
   AuthPeersLambdas getAuthPeerLambda() const;
   void processAEADHandshake(BinaryData& msg, const std::string& clientID);

   std::shared_ptr<AuthorizedPeers> authPeers_;
   std::shared_ptr<BIP151Connection> bip151Connection_;
   std::chrono::time_point<std::chrono::system_clock> outKeyTimePoint_;
   const uint64_t id_;
   bool bip15XHandshakeCompleted_ = false;
};
#endif // __ZMQ_BIP15X_SERVERCONNECTION_H__
