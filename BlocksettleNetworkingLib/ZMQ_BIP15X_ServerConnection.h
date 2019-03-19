#ifndef __ZMQ_BIP15X_SERVERCONNECTION_H__
#define __ZMQ_BIP15X_SERVERCONNECTION_H__

#include <QString>
#include <spdlog/spdlog.h>
#include "AuthorizedPeers.h"
#include "BIP150_151.h"
#include "EncryptionUtils.h"
#include "ZmqServerConnection.h"
#include "ZMQ_BIP15X_Msg.h"

#define SERVER_AUTH_PEER_FILENAME "server.peers"

class zmqBIP15XServerConnection : public ZmqServerConnection
{
public:
   zmqBIP15XServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , const QStringList& trustedClients, const uint64_t& id
      , const bool& ephemeralPeers);
   zmqBIP15XServerConnection(const zmqBIP15XServerConnection&) = delete;
   zmqBIP15XServerConnection& operator= (const zmqBIP15XServerConnection&) = delete;
   zmqBIP15XServerConnection(zmqBIP15XServerConnection&&) = delete;
   zmqBIP15XServerConnection& operator= (zmqBIP15XServerConnection&&) = delete;

   // Overridden functions from ServerConnection.
   bool SendDataToClient(const std::string& clientId, const std::string& data
      , const SendResultCb &cb = nullptr) override;

   void resetBIP151Connection(const std::string& clientID);
   void setBIP151Connection(const std::string& clientID);
   bool handshakeCompleted() {
      return (bip150HandshakeCompleted_ && bip151HandshakeCompleted_);
   }

protected:
   // Overridden functions from ZmqServerConnection.
   ZmqContext::sock_ptr CreateDataSocket() override;
   bool ConfigDataSocket(const ZmqContext::sock_ptr& dataSocket) override;
   bool ReadFromDataSocket() override;

private:
   void ProcessIncomingData(const std::string& encData
      , const std::string& clientID);
   bool processAEADHandshake(const BinaryData& msgObj
      , const std::string& clientID);
   void promptUser(const BinaryDataRef& newKey, const std::string& srvAddrPort);
   AuthPeersLambdas getAuthPeerLambda();

   std::shared_ptr<AuthorizedPeers> authPeers_;
   std::unique_ptr<BIP151Connection> bip151Connection_;
   std::map<std::string, std::unique_ptr<BIP151Connection>> socketConnMap_; // Socket & connection
   std::chrono::time_point<std::chrono::system_clock> outKeyTimePoint_;
   const uint64_t id_;
   bool bip150HandshakeCompleted_ = false;
   bool bip151HandshakeCompleted_ = false;
   QStringList trustedClients_;
};
#endif // __ZMQ_BIP15X_SERVERCONNECTION_H__
