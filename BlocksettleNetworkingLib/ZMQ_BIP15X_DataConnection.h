#ifndef __ZMQ_BIP15X_DATACONNECTION_H__
#define __ZMQ_BIP15X_DATACONNECTION_H__

#include <deque>
#include <functional>
#include <mutex>
#include <spdlog/spdlog.h>
#include <thread>

#include "AuthorizedPeers.h"
#include "BIP150_151.h"
#include "ZMQ_BIP15X_Helpers.h"
#include "ZmqDataConnection.h"

template<typename T> class FutureValue;

// DESIGN NOTES: Remote data connections must have a callback for when unknown
// server keys are seen. The callback should ask the user if they'll accept
// the new key, or otherwise properly handle new keys arriving.
//
// Cookies are used for local connections, and are the default unless remote
// callbacks are added. When the server is invoked by a binary containing a
// client connection, the binary must be invoked with the client connection's
// public BIP 150 ID key. In turn, the binary with the server connection must
// generate a cookie with its public BIP 150 ID key. The client will read the
// cookie and get the server key. This allows both sides to verify each other.
//
// When adding authorized keys to a connection, the name needs to be the
// IP:Port of the server connection. This is the only reliable information
// available to the connection that can be used to ID who's on the other side.
// It's okay to use other names in the GUI and elsewhere. However, the IP:Port
// must be used when searching for keys.
//
// The key acceptance functionality is as follows:
//
// LOCAL SIGNER
// Accept only a single key from the server cookie.
//
// REMOTE SIGNER
// New key + No callbacks - Reject the new keys.
// New key + Callbacks - Depends on what the user wants.
// Previously verified key - Accept the key and skip the callbacks.

enum class BIP15XCookie
{
   // Cookie won't be used
   NotUsed,

   // Connection will make a key cookie
   MakeClient,

   // Connection will read a key cookie (server's public key)
   ReadServer,
};

struct ZmqBIP15XDataConnectionParams
{

   // The directory containing the file with the non-ephemeral key
   std::string ownKeyFileDir;

   // The file name with the non-ephemeral key
   std::string ownKeyFileName;

   // File where cookie will be stored or read from.
   // Must be set cookie is used.
   std::string cookiePath{};

   // Ephemeral peer usage. Not recommended
   bool ephemeralPeers{false};

   BIP15XCookie cookie{BIP15XCookie::NotUsed};

   // Initialized to ZmqBIP15XServerConnection::getDefaultHeartbeatInterval() by default
   std::chrono::milliseconds heartbeatInterval{};

   std::chrono::milliseconds connectionTimeout{std::chrono::seconds(10)};

   ZmqBIP15XDataConnectionParams();

   void setLocalHeartbeatInterval();
};

class ZmqBipMsg;

class ZmqBIP15XDataConnection : public DataConnection
{
public:
   ZmqBIP15XDataConnection(const std::shared_ptr<spdlog::logger>& logger, const ZmqBIP15XDataConnectionParams &params);
   ~ZmqBIP15XDataConnection() noexcept override;

   ZmqBIP15XDataConnection(const ZmqBIP15XDataConnection&) = delete;
   ZmqBIP15XDataConnection& operator= (const ZmqBIP15XDataConnection&) = delete;
   ZmqBIP15XDataConnection(ZmqBIP15XDataConnection&&) = delete;
   ZmqBIP15XDataConnection& operator= (ZmqBIP15XDataConnection&&) = delete;

   bool getServerIDCookie(BinaryData& cookieBuf);
   std::string getCookiePath() const { return params_.cookiePath; }
   void setCBs(const ZmqBipNewKeyCb& inNewKeyCB);
   BinaryData getOwnPubKey() const;
   bool genBIPIDCookie();
   void addAuthPeer(const ZmqBIP15XPeer &peer);
   void updatePeerKeys(const ZmqBIP15XPeers &peers);

   // Could be called from callbacks and control thread (where openConnection was called, main thread usually)
   bool send(const std::string& data) override;

   bool openConnection(const std::string &host, const std::string &port
      , DataConnectionListener *) override;

   // Do not call from callbacks!
   bool closeConnection() override;

   // Only for tests
   void rekey();

   bool isActive() const;
   bool SetZMQTransport(ZMQTransport transport);

   static BinaryData getOwnPubKey(const std::string &ownKeyFileDir, const std::string &ownKeyFileName);
   static BinaryData getOwnPubKey(const AuthorizedPeers &authPeers);
private:
   enum class InternalCommandCode
   {
      Invalid,
      Send,
      Stop,
   };

   bool startBIP151Handshake();
   bool handshakeCompleted() {
      return (bip150HandshakeCompleted_ && bip151HandshakeCompleted_);
   }

   // Use to send a packet that this class has generated.
   void sendPacket(const BinaryData& data);

   void onRawDataReceived(const std::string& rawData) override;

   ZmqContext::sock_ptr CreateDataSocket();
   bool recvData();
   void triggerHeartbeatCheck();

   void onConnected();
   void onDisconnected();
   void onError(DataConnectionListener::DataConnectionError errorCode);

   void ProcessIncomingData(BinaryData& payload);
   bool processAEADHandshake(const ZmqBipMsg& msgObj);
   bool verifyNewIDKey(const BinaryDataRef& newKey
      , const std::string& srvAddrPort);
   AuthPeersLambdas getAuthPeerLambda() const;
   void rekeyIfNeeded(size_t dataSize);
   void listenFunction();
   void resetConnectionObjects();
   bool ConfigureDataSocket(const ZmqContext::sock_ptr& socket);
   void sendCommand(InternalCommandCode command);
   void sendPendingData();
   void sendDisconnectMsg();

   std::shared_ptr<spdlog::logger>  logger_;
   const ZmqBIP15XDataConnectionParams params_;

   std::shared_ptr<FutureValue<bool>> serverPubkeyProm_;
   std::unique_ptr<AuthorizedPeers> authPeers_;
   mutable std::mutex authPeersMutex_;
   std::unique_ptr<BIP151Connection> bip151Connection_;
   std::chrono::time_point<std::chrono::steady_clock> outKeyTimePoint_;
   uint32_t outerRekeyCount_ = 0;
   uint32_t innerRekeyCount_ = 0;
   bool bip150HandshakeCompleted_ = false;
   bool bip151HandshakeCompleted_ = false;

   ZmqBipNewKeyCb cbNewKey_;

   std::shared_ptr<ZmqContext>      context_;

   ZmqContext::sock_ptr             dataSocket_;
   ZmqContext::sock_ptr             monSocket_;
   ZmqContext::sock_ptr             threadMasterSocket_;
   ZmqContext::sock_ptr             threadSlaveSocket_;

   std::string                      connectionName_;
   std::string                      hostAddr_;
   std::string                      hostPort_;
   std::string                      socketId_;

   std::thread                      listenThread_;

   ZMQTransport                     zmqTransport_ = ZMQTransport::TCPTransport;

   std::vector<std::string>         pendingData_;
   std::mutex                       pendingDataMutex_;

   // Reset this in openConnection
   bool                             isConnected_{};
   bool                             fatalError_{};
   bool                             serverSendsHeartbeat_{};
   std::chrono::steady_clock::time_point lastHeartbeatSend_{};
   std::chrono::steady_clock::time_point lastHeartbeatReply_{};
};

#endif // __ZMQ_BIP15X_DATACONNECTION_H__
