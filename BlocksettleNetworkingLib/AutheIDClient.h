#ifndef __AUTH_EID_CLIENT_H__
#define __AUTH_EID_CLIENT_H__

#include <QObject>
#include <QTimer>

#include "DataConnectionListener.h"
#include "ZmqSecuredDataConnection.h"
#include "EncryptUtils.h"

#include "rp_api.pb.h"


namespace spdlog {
   class logger;
}

class ConnectionManager;


class AutheIDClient : public QObject
                    , DataConnectionListener
{
    Q_OBJECT

public:
    AutheIDClient(const std::shared_ptr<spdlog::logger> &
       , const std::pair<autheid::PrivateKey, autheid::PublicKey> &
       , QObject *parent = nullptr);
    ~AutheIDClient() noexcept override;

    bool authenticate(const std::string email);
    void connectToAuthServer(const std::string &serverPubKey
       , const std::string &serverHost, const std::string &serverPort);

private:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   bool sendToAuthServer(const std::string &payload, const AutheID::RP::PayloadType type);
   void processResultReply(const uint8_t *payload, size_t payloadSize);
   void cancel();

signals:
   void failed(const QString &text);

private slots:
   void timeout();

private:
   std::unique_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<ZmqSecuredDataConnection> connection_;
   std::shared_ptr<spdlog::logger> logger_;
   std::string requestId_;
   std::string email_;

   const std::pair<autheid::PrivateKey, autheid::PublicKey> authKeys_;

   QScopedPointer<QTimer> timer_;

};

#endif
