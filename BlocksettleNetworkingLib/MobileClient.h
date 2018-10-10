#ifndef __MOBILE_CLIENT_H__
#define __MOBILE_CLIENT_H__

#include <cryptopp/rsa.h>
#include <QObject>
#include "DataConnectionListener.h"
#include "EncryptionUtils.h"
#include "ZmqSecuredDataConnection.h"

namespace spdlog {
   class logger;
}

namespace Blocksettle { namespace AuthServer {
class Packet;
} }

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
   bool start(const std::string &email, const std::string &walletId);
   void cancel();

signals:
   void succeeded(const SecureBinaryData &password);
   void failed(const QString &text);

private:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   void processGetKeyResponse(const Blocksettle::AuthServer::Packet &packet);

   std::unique_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<ZmqSecuredDataConnection> connection_;
   std::shared_ptr<spdlog::logger> logger_;
   uint64_t tag_{};
   std::string email_;
   std::string walletId_;
   std::string data_;

   CryptoPP::RSA::PrivateKey privateKey_;
   std::string publicKey_;
};

#endif // __MOBILE_CLIENT_H__
