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

// A struct containing the data required per-connection with clients.
struct ZmqBIP15XPerConnData
{
public:
   void reset();

   std::unique_ptr<BIP151Connection> encData_;
   bool bip150HandshakeCompleted_ = false;
   bool bip151HandshakeCompleted_ = false;
   std::chrono::time_point<std::chrono::system_clock> outKeyTimePoint_;
};

// The class establishing ZMQ sockets and establishing BIP 150/151 handshakes
// before encrypting/decrypting the on-the-wire data using BIP 150/151.
class ZmqBIP15XServerConnection : public ZmqServerConnection
{
public:
   ZmqBIP15XServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , const QStringList& trustedClients, const uint64_t& id
      , const bool& ephemeralPeers);
   ZmqBIP15XServerConnection(const ZmqBIP15XServerConnection&) = delete;
   ZmqBIP15XServerConnection& operator= (const ZmqBIP15XServerConnection&) = delete;
   ZmqBIP15XServerConnection(ZmqBIP15XServerConnection&&) = delete;
   ZmqBIP15XServerConnection& operator= (ZmqBIP15XServerConnection&&) = delete;

   // Overridden functions from ServerConnection.
   bool SendDataToClient(const std::string& clientId, const std::string& data
      , const SendResultCb& cb = nullptr) override;

   void resetBIP151Connection(const std::string& clientID);
   void setBIP151Connection(const std::string& clientID);
   bool handshakeCompleted(const ZmqBIP15XPerConnData& checkConn) {
      return (checkConn.bip150HandshakeCompleted_ &&
         checkConn.bip151HandshakeCompleted_);
   }

protected:
   // Overridden functions from ZmqServerConnection.
   ZmqContext::sock_ptr CreateDataSocket() override;
   bool ReadFromDataSocket() override;

private:
   void ProcessIncomingData(const std::string& encData
      , const std::string& clientID);
   bool processAEADHandshake(const BinaryData& msgObj
      , const std::string& clientID);
   void promptUser(const BinaryDataRef& newKey, const std::string& srvAddrPort);
   AuthPeersLambdas getAuthPeerLambda();

   std::shared_ptr<AuthorizedPeers> authPeers_;
   std::map<std::string, std::unique_ptr<ZmqBIP15XPerConnData>> socketConnMap_;
   const uint64_t id_;
   QStringList trustedClients_;
};
#endif // __ZMQ_BIP15X_SERVERCONNECTION_H__
