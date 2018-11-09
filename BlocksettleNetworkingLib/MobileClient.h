#ifndef __MOBILE_CLIENT_H__
#define __MOBILE_CLIENT_H__

#include <botan/ecies.h>
#include <botan/auto_rng.h>
#include <QObject>
#include "DataConnectionListener.h"
#include "EncryptionUtils.h"
#include "MobileClientRequestType.h"
#include "ZmqSecuredDataConnection.h"
#include "EncryptUtils.h"
#include "rp_api.pb.h"

namespace spdlog {
   class logger;
}

class QTimer;
class ConnectionManager;
class RequestReplyCommand;

class MobileClient : public QObject, public DataConnectionListener
{
   Q_OBJECT
public:
   struct DeviceInfo
   {
      std::string userId;
      std::string deviceId;
      std::string deviceName;
   };

   static DeviceInfo getDeviceInfo(const std::string &encKey);

   MobileClient(const std::shared_ptr<spdlog::logger> &
      , const std::pair<autheid::PrivateKey, autheid::PublicKey> &
      , QObject *parent = nullptr);
   ~MobileClient() override;

   void init(const std::string &serverPubKey
      , const std::string &serverHost, const std::string &serverPort);
   bool start(MobileClientRequest requestType, const std::string &email, const std::string &walletId
      , const std::vector<std::string> &knownDeviceIds);
   void cancel();

signals:
   void succeeded(const std::string& encKey, const SecureBinaryData &password);
   void failed(const QString &text);

private slots:
   void timeout();

private:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   bool sendToAuthServer(const std::string &payload, const AutheID::RP::PayloadType type);
   void processCreateReply(const uint8_t *payload, size_t payloadSize);
   void processResultReply(const uint8_t *payload, size_t payloadSize);

   static std::string toBase64(const std::string &);
   static std::vector<uint8_t> fromBase64(const std::string &);

   std::unique_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<ZmqSecuredDataConnection> connection_;
   std::shared_ptr<spdlog::logger> logger_;
   std::string requestId_;
   std::string email_;
   std::string walletId_;

   std::string serverPubKey_;
   std::string serverHost_;
   std::string serverPort_;

   const std::pair<autheid::PrivateKey, autheid::PublicKey> authKeys_;

   QTimer *timer_;

   bool isConnecting_{false};
   std::vector<std::string> knownDeviceIds_;
};

#endif // __MOBILE_CLIENT_H__
