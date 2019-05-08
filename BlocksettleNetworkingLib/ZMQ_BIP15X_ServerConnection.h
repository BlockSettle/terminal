#ifndef __ZMQ_BIP15X_SERVERCONNECTION_H__
#define __ZMQ_BIP15X_SERVERCONNECTION_H__

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>
#include "AuthorizedPeers.h"
#include "BIP150_151.h"
#include "EncryptionUtils.h"
#include "ZmqServerConnection.h"
#include "ZMQ_BIP15X_Msg.h"

#define SERVER_AUTH_PEER_FILENAME "server.peers"

// DESIGN NOTES: Cookies are used for local connections. When the client is
// invoked by a binary containing a server connection, the binary must be
// invoked with the client connection's public BIP 150 ID key. In turn, the
// binary with the client connection must generate a cookie with its public BIP
// 150 ID key. The server will read the cookie and get the client key. This
// allows both sides to verify each other.
//
// When adding authorized keys to a connection, the name needs to be the ZMQ
// client ID. This is because the ID is the only reliable information that's
// available and can be used to ID who's on the other side of a connection. It's
// okay to use other names in the GUI and elsewhere. However, the client ID must
// be used when searching for keys.

// A struct containing the data required per-connection with clients.
struct ZmqBIP15XPerConnData
{
public:
   void reset();

   std::unique_ptr<BIP151Connection> encData_;
   bool bip150HandshakeCompleted_ = false;
   bool bip151HandshakeCompleted_ = false;
   std::chrono::time_point<std::chrono::steady_clock> outKeyTimePoint_;
   uint32_t msgID_ = 0;
   uint32_t outerRekeyCount_ = 0;
   uint32_t innerRekeyCount_ = 0;
   ZmqBIP15XMsgFragments currentReadMessage_;
};

// The class establishing ZMQ sockets and establishing BIP 150/151 handshakes
// before encrypting/decrypting the on-the-wire data using BIP 150/151. Used by
// the server in a connection.
class ZmqBIP15XServerConnection : public ZmqServerConnection
{
public:
   ZmqBIP15XServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , const std::vector<std::string>& trustedClients
      , const uint64_t& id
      , const bool& ephemeralPeers
      , const bool& makeServerCookie = false
      , const bool& readClientCookie = false
      , const std::string& cookiePath = "");
   ZmqBIP15XServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , const std::function<std::vector<std::string>()>& cbTrustedClients
      , const bool& makeServerCookie = false
      , const bool& readClientCookie = false
      , const std::string& cookiePath = "");
   ~ZmqBIP15XServerConnection() noexcept override;

   ZmqBIP15XServerConnection(const ZmqBIP15XServerConnection&) = delete;
   ZmqBIP15XServerConnection& operator= (const ZmqBIP15XServerConnection&) = delete;
   ZmqBIP15XServerConnection(ZmqBIP15XServerConnection&&) = delete;
   ZmqBIP15XServerConnection& operator= (ZmqBIP15XServerConnection&&) = delete;

   // Overridden functions from ServerConnection.
   bool SendDataToClient(const std::string& clientId, const std::string& data
      , const SendResultCb& cb = nullptr) override;
   bool SendDataToAllClients(const std::string&, const SendResultCb &cb = nullptr) override;

   bool getClientIDCookie(BinaryData& cookieBuf);
   std::string getCookiePath() const { return bipIDCookiePath_; }
   BinaryData getOwnPubKey() const;
   void addAuthPeer(const BinaryData& inKey, const std::string& keyName);

   void rekey(const std::string &clientId);
   void setLocalHeartbeatInterval();

   static const std::chrono::milliseconds DefaultHeartbeatInterval;
   static const std::chrono::milliseconds LocalHeartbeatInterval;

protected:
   // Overridden functions from ZmqServerConnection.
   ZmqContext::sock_ptr CreateDataSocket() override;
   bool ReadFromDataSocket() override;

   void resetBIP151Connection(const std::string& clientID);
   void setBIP151Connection(const std::string& clientID);
   bool handshakeCompleted(const ZmqBIP15XPerConnData& checkConn) {
      return (checkConn.bip150HandshakeCompleted_ &&
         checkConn.bip151HandshakeCompleted_);
   }

private:
   void ProcessIncomingData(const std::string& encData
      , const std::string& clientID);
   bool processAEADHandshake(const ZmqBIP15XMsgPartial& msgObj
      , const std::string& clientID);
   AuthPeersLambdas getAuthPeerLambda();
   bool genBIPIDCookie();
   void heartbeatThread();

   std::shared_ptr<AuthorizedPeers> authPeers_;
   std::map<std::string, std::unique_ptr<ZmqBIP15XPerConnData>> socketConnMap_;
   BinaryData leftOverData_;
   bool bipIDCookieExists_ = false;
   uint64_t id_;
   std::mutex  clientsMtx_;
   std::function<std::vector<std::string>()> cbTrustedClients_;
   const bool useClientIDCookie_;
   const bool makeServerIDCookie_;
   const std::string bipIDCookiePath_;

   std::unordered_map<std::string, std::chrono::steady_clock::time_point>  lastHeartbeats_;
   std::atomic_bool        hbThreadRunning_;
   std::thread             hbThread_;
   std::mutex              hbMutex_;
   std::condition_variable hbCondVar_;

   std::mutex              rekeyMutex_;
   std::unordered_set<std::string>  rekeyStarted_;
   std::unordered_map<std::string, std::vector<std::tuple<std::string, SendResultCb>>> pendingData_;
   std::chrono::milliseconds heartbeatInterval_;
};
#endif // __ZMQ_BIP15X_SERVERCONNECTION_H__
