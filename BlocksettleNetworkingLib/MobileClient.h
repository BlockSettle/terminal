#ifndef __MOBILE_CLIENT_H__
#define __MOBILE_CLIENT_H__

#include <cryptopp/rsa.h>
#include <QObject>
#include "DataConnectionListener.h"
#include "EncryptionUtils.h"
#include "MobileClientRequestType.h"
#include "ZmqSecuredDataConnection.h"
#include "rp_api.pb.h"

namespace spdlog {
   class logger;
}

class ConnectionManager;
class RequestReplyCommand;

class MobileClient : public QObject, public DataConnectionListener
{
   Q_OBJECT
public:
   MobileClient(const std::shared_ptr<spdlog::logger> &logger, QObject *parent = nullptr);
   ~MobileClient() override;

   void init(const std::string &serverPubKey
      , const std::string &serverHost, const std::string &serverPort);
   bool start(MobileClientRequest requestType, const std::string &email, const std::string &walletId);
   void cancel();

   void updateServer(const std::string &deviceId, const std::string &walletId
      , bool isPaired, bool deleteAll);

signals:
   void succeeded(const std::string& deviceId, const SecureBinaryData &password);
   void failed(const QString &text);
   void updateServerFinished(bool success);

private:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   bool sendToAuthServer(const std::string &payload, const AutheID::RP::EnvelopeRequestType);
   void processGetKeyReply(const std::string &payload, uint64_t tag);
   void processUpdateDeviceWalletReply(const std::string &payload, uint64_t tag);

   static std::string toBase64(const std::string &);
   static std::string fromBase64(const std::string &);

   std::unique_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<ZmqSecuredDataConnection> connection_;
   std::shared_ptr<spdlog::logger> logger_;
   uint64_t tag_{};
   std::string email_;
   std::string walletId_;
   CryptoPP::AutoSeededRandomPool rng_;

   CryptoPP::RSA::PrivateKey privateKey_;
   std::string publicKey_;
   const SecureBinaryData ownPrivKey_;
   const SecureBinaryData serverPubKey_;
};

#endif // __MOBILE_CLIENT_H__
