#ifndef __ZMQ_BIP15X_DATACONNECTION_H__
#define __ZMQ_BIP15X_DATACONNECTION_H__

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>
#include "AuthorizedPeers.h"
#include "BIP150_151.h"
#include "ZmqDataConnection.h"
#include "ZMQ_BIP15X_Msg.h"

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

class ZmqBIP15XDataConnection : public ZmqDataConnection
{
public:
   ZmqBIP15XDataConnection(const std::shared_ptr<spdlog::logger>& logger
      , const bool ephemeralPeers = false, const std::string& ownKeyFileDir = ""
      , const std::string& ownKeyFileName = "", const bool monitored = false
      , const bool makeClientCookie = false, const bool readServerCookie = false
      , const std::string& cookiePath = "");
   ~ZmqBIP15XDataConnection() noexcept override;

   using cbNewKey = std::function<void(const std::string &oldKey, const std::string &newKey
      , const std::string& srvAddrPort, const std::shared_ptr<std::promise<bool>> &prompt)>;
   using invokeCB = std::function<void(const std::string&
      , const std::string&
      , std::shared_ptr<std::promise<bool>>
      , const cbNewKey&)>;

   ZmqBIP15XDataConnection(const ZmqBIP15XDataConnection&) = delete;
   ZmqBIP15XDataConnection& operator= (const ZmqBIP15XDataConnection&) = delete;
   ZmqBIP15XDataConnection(ZmqBIP15XDataConnection&&) = delete;
   ZmqBIP15XDataConnection& operator= (ZmqBIP15XDataConnection&&) = delete;

   bool getServerIDCookie(BinaryData& cookieBuf);
   std::string getCookiePath() const { return bipIDCookiePath_; }
   void setCBs(const cbNewKey& inNewKeyCB);
   BinaryData getOwnPubKey() const;
   bool genBIPIDCookie();
   void addAuthPeer(const BinaryData& inKey, const std::string& inKeyName);
   void updatePeerKeys(const std::vector<std::pair<std::string, BinaryData>> &);
   void setLocalHeartbeatInterval();

   // Overridden functions from ZmqDataConnection.
   bool send(const std::string& data) override; // Send data from outside class.
   bool openConnection(const std::string &host, const std::string &port
      , DataConnectionListener *) override;
   bool closeConnection() override;

   void rekey();

protected:
   bool startBIP151Handshake(const std::function<void()> &cbCompleted);
   bool handshakeCompleted() {
      return (bip150HandshakeCompleted_ && bip151HandshakeCompleted_);
   }

   // Use to send a packet that this class has generated.
   bool sendPacket(const std::string& data);

   // Overridden functions from ZmqDataConnection.
   void onRawDataReceived(const std::string& rawData) override;
   void notifyOnConnected() override;
   ZmqContext::sock_ptr CreateDataSocket() override;
   bool recvData() override;
   void triggerHeartbeat();

   void notifyOnError(DataConnectionListener::DataConnectionError errorCode);

private:
   void ProcessIncomingData(BinaryData& payload);
   bool processAEADHandshake(const ZmqBIP15XMsgPartial& msgObj);
   bool verifyNewIDKey(const BinaryDataRef& newKey
      , const std::string& srvAddrPort);
   AuthPeersLambdas getAuthPeerLambda() const;
   void rekeyIfNeeded(size_t dataSize);

private:
   std::shared_ptr<std::promise<bool>> serverPubkeyProm_;
   bool  serverPubkeySignalled_ = false;
   std::shared_ptr<AuthorizedPeers> authPeers_;
   std::shared_ptr<BIP151Connection> bip151Connection_;
   std::chrono::time_point<std::chrono::steady_clock> outKeyTimePoint_;
   uint32_t outerRekeyCount_ = 0;
   uint32_t innerRekeyCount_ = 0;
   ZmqBIP15XMsgFragments currentReadMessage_;
   BinaryData leftOverData_;
   std::atomic_flag lockSocket_ = ATOMIC_FLAG_INIT;
   bool bip150HandshakeCompleted_ = false;
   bool bip151HandshakeCompleted_ = false;
   const std::string bipIDCookiePath_;
   const bool useServerIDCookie_;
   const bool makeClientIDCookie_;
   uint32_t msgID_ = 0;
   std::function<void()>   cbCompleted_ = nullptr;

   cbNewKey cbNewKey_;

   std::atomic<std::chrono::steady_clock::time_point> lastHeartbeatReply_;
   std::atomic_bool        hbThreadRunning_;
   std::thread             hbThread_;
   std::mutex              hbMutex_;
   std::condition_variable hbCondVar_;
   std::atomic_bool        fatalError_{false};
   std::atomic_bool        serverSendsHeartbeat_{false};
   std::chrono::milliseconds heartbeatInterval_;
};

#endif // __ZMQ_BIP15X_DATACONNECTION_H__
