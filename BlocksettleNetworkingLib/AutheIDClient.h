#ifndef __AUTH_EID_CLIENT_H__
#define __AUTH_EID_CLIENT_H__

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <functional>

#include "EncryptionUtils.h"
#include "autheid_utils.h"

namespace spdlog {
   class logger;
}

namespace autheid {
   namespace rp {
   class GetResultResponse_SignatureResult;
   }
}

class ApplicationSettings;
class QNetworkReply;
class ConnectionManager;

class AutheIDClient : public QObject
{
   Q_OBJECT

public:
   struct DeviceInfo
   {
      std::string userId;
      std::string deviceId;
      std::string deviceName;
   };

   enum RequestType
   {
      Unknown,
      ActivateWallet,
      DeactivateWallet,
      SignWallet,
      BackupWallet,
      ActivateWalletOldDevice,
      ActivateWalletNewDevice,
      DeactivateWalletDevice,
      VerifyWalletKey,
      ActivateOTP,
      // Private market and others with lower timeout
      SettlementTransaction,

      // Please also add new type text in getAutheIDClientRequestText
   };
   Q_ENUM(RequestType)

   enum ErrorType
   {
      NoError,
      CreateError,
      DecodeError,
      DecryptError,
      InvalidSecureReplyError,
      InvalidKeySizeError,
      MissingSignatuteError,
      SerializationSignatureError,
      ParseSignatureError,
      Timeout,
      Cancelled,
      NotAuthenticated,
      ServerError
   };
   Q_ENUM(ErrorType)
   static QString errorString(ErrorType error);

   static DeviceInfo getDeviceInfo(const std::string &encKey);

   // ConnectionManager must live long enough to be able send cancel message
   // (if cancelling request in mobile app is needed)
   AutheIDClient(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ConnectionManager> &, QObject *parent = nullptr);
   ~AutheIDClient() override;

   void start(RequestType requestType, const std::string &email, const std::string &walletId
      , const std::vector<std::string> &knownDeviceIds, int expiration = 120);
   void sign(const BinaryData &data, const std::string &email
      , const QString &title, const QString &description, int expiration = 30);
   void authenticate(const std::string &email, int expiration = 120);
   void cancel();

signals:
   void succeeded(const std::string& encKey, const SecureBinaryData &password);
   void signSuccess(const std::string &data, const BinaryData &invisibleData, const std::string &signature);
   void authSuccess(const std::string &jwt);
   void failed(QNetworkReply::NetworkError networkError, ErrorType error);
   void userCancelled();

private:
   struct Result
   {
      QByteArray payload;
      ErrorType authError;
      QNetworkReply::NetworkError networkError;
      //std::string errorMsg;
   };

   using ResultCallback = std::function<void(const Result &result)>;

   void requestAuth(const std::string& email, int expiration);
   void createCreateRequest(const std::string &payload, int expiration);
   void processCreateReply(const QByteArray &payload, int expiration);
   void processResultReply(const QByteArray &payload);

   void processNetworkReply(QNetworkReply *, int timeoutSeconds, const ResultCallback &);

   void processSignatureReply(const autheid::rp::GetResultResponse_SignatureResult &);

   QString getAutheIDClientRequestText(RequestType requestType);
   bool isAutheIDClientNewDeviceNeeded(RequestType requestType);

private:
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<ApplicationSettings> settings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::string requestId_;
   std::string email_;
   bool resultAuth_{};

   const std::pair<autheid::PrivateKey, autheid::PublicKey> authKeys_;

   std::vector<std::string> knownDeviceIds_;

   const char *baseUrl_;
   const char *apiKey_;
};
Q_DECLARE_METATYPE(AutheIDClient::RequestType)

#endif // __AUTH_EID_CLIENT_H__
