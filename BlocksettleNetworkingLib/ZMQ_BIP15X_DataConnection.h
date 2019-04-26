#ifndef __ZMQ_BIP15X_DATACONNECTION_H__
#define __ZMQ_BIP15X_DATACONNECTION_H__

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>
#include "AuthorizedPeers.h"
#include "ArmoryServersProvider.h"
#include "BIP150_151.h"
#include "ZmqDataConnection.h"
#include "ZMQ_BIP15X_Msg.h"

#define CLIENT_AUTH_PEER_FILENAME "client.peers"

// DESIGN NOTE: The data connection should have a callback for when unknown
// server keys are seen. The callback should ask the user if they'll accept
// the new key. This is meant for remote connections.
//
// In adition, we have cookies that are used for local connections. When the
// server is invoked, it'll be invoked with the client connection's public BIP
// 150 ID key. In turn, the server will generate a cookie with its public BIP
// 150 ID key. The client will read the cookie and get the server key. This
// allows both sides to verify each other.
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
      , const bool& ephemeralPeers = false, const bool& monitored = false
      , const bool& genIDCookie = false);
/*   ZmqBIP15XDataConnection(const std::shared_ptr<spdlog::logger>& logger
      , const ArmoryServersProvider& trustedServer, const bool& ephemeralPeers
      , bool monitored);*/
   ~ZmqBIP15XDataConnection() noexcept override;

   using cbNewKey = std::function<void(const std::string&, const std::string&
      , std::shared_ptr<std::promise<bool>>)>;
   using invokeCB = std::function<void(const std::string&
      , const std::string&
      , std::shared_ptr<std::promise<bool>>
      , const cbNewKey&)>;

   ZmqBIP15XDataConnection(const ZmqBIP15XDataConnection&) = delete;
   ZmqBIP15XDataConnection& operator= (const ZmqBIP15XDataConnection&) = delete;
   ZmqBIP15XDataConnection(ZmqBIP15XDataConnection&&) = delete;
   ZmqBIP15XDataConnection& operator= (ZmqBIP15XDataConnection&&) = delete;

   bool getServerIDCookie(BinaryData& cookieBuf, const std::string& cookieName);
   void setCBs(const cbNewKey& inNewKeyCB);
   BinaryData getOwnPubKey() const;
   bool genBIPIDCookie();
   void addAuthPeer(const BinaryData& inKey, const std::string& inKeyName);

   // Overridden functions from ZmqDataConnection.
   bool send(const std::string& data) override; // Send data from outside class.
   bool closeConnection() override;

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

private:
   void ProcessIncomingData(BinaryData& payload);
   bool processAEADHandshake(const ZmqBIP15XMsgPartial& msgObj);
   void verifyNewIDKey(const BinaryDataRef& newKey
      , const std::string& srvAddrPort);
   AuthPeersLambdas getAuthPeerLambda() const;
   void rekeyIfNeeded(const size_t& dataSize);

   std::shared_ptr<std::promise<bool>> serverPubkeyProm_;
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
   bool useServerIDCookie_ = true;
   bool bipIDCookieExists_ = false;
   uint32_t msgID_ = 0;
   std::function<void()>   cbCompleted_ = nullptr;
   const int   heartbeatInterval_ = 30000;

   cbNewKey cbNewKey_;

   std::chrono::steady_clock::time_point  lastHeartbeat_;
   std::atomic_bool        hbThreadRunning_;
   std::thread             hbThread_;
   std::mutex              hbMutex_;
   std::condition_variable hbCondVar_;
};

#endif // __ZMQ_BIP15X_DATACONNECTION_H__
