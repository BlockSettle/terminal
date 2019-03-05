#include "AutheIDClient.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include <spdlog/spdlog.h>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "rp.pb.h"

using namespace autheid;

namespace
{
   const int kNetworkTimeoutSeconds = 10;

   const int kKeySize = 32;

   static const char kSeparatorSymbol = ':';

   const auto kContentTypeHeader = "Content-Type";
   const auto kAcceptHeader = "Accept";
   const auto kAuthorizationHeader = "Authorization";

   const auto kProtobufType = "application/protobuf";
   const auto kAuthorizationKey = "Bearer Pj+Q9SsZloftMkmE7EhA8v2Bz1ZC9aOmUkAKTBW9hagJ";

   QNetworkRequest getRequest(const std::string &url)
   {
      QNetworkRequest request;
      request.setUrl(QUrl(QString::fromStdString(url)));
      request.setRawHeader(kContentTypeHeader, kProtobufType);
      request.setRawHeader(kAcceptHeader, kProtobufType);
      request.setRawHeader(kAuthorizationHeader, kAuthorizationKey);
      return request;
   }
} // namespace

AutheIDClient::DeviceInfo AutheIDClient::getDeviceInfo(const std::string &encKey)
{
   DeviceInfo result;

   // encKey could be in form email, email:deviceId and email:deviceId:deviceName formats
   size_t firstSep = encKey.find(kSeparatorSymbol);
   if (firstSep != std::string::npos) {
      size_t secondSep = encKey.find(kSeparatorSymbol, firstSep + 1);
      size_t deviceIdLen = std::string::npos;
      if (secondSep != std::string::npos) {
         deviceIdLen = secondSep - firstSep - 1;
         result.deviceName = encKey.substr(secondSep + 1);
      }
      result.deviceId = encKey.substr(firstSep + 1, deviceIdLen);
   }

   result.userId = encKey.substr(0, firstSep);
   return result;
}

AutheIDClient::AutheIDClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &settings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , settings_(settings)
   , connectionManager_(connectionManager)
   , resultAuth_(false)
   , authKeys_(settings->GetAuthKeys())
{
   EnvConfiguration conf = EnvConfiguration(settings_->get<int>(ApplicationSettings::envConfiguration));

   switch (conf) {
      case EnvConfiguration::UAT:
      case EnvConfiguration::Staging:
      case EnvConfiguration::Custom:
         baseUrl_ = "https://api.staging.autheid.com/v1/requests";
         break;
      default:
         baseUrl_ = "https://api.autheid.com/v1/requests";
         break;
   }
}

AutheIDClient::~AutheIDClient()
{
   cancel();
}

void AutheIDClient::createCreateRequest(const std::string &payload, int expiration)
{
   QNetworkRequest request = getRequest(baseUrl_);

   QNetworkReply *reply = connectionManager_->GetNAM()->post(request, QByteArray::fromStdString(payload));
   processNetworkReply(reply, kNetworkTimeoutSeconds, [this, expiration] (const Result &result) {
      if (!result.success) {
         emit failed(tr("Auth eID faild: %1").arg(QString::fromStdString(result.errorMsg)));
         return;
      }

      processCreateReply(result.payload, expiration);
   });
}

void AutheIDClient::start(RequestType requestType, const std::string &email
   , const std::string &walletId, const std::vector<std::string> &knownDeviceIds)
{
   cancel();

   email_ = email;

   QString action = getAutheIDClientRequestText(requestType);
   bool newDevice = isAutheIDClientNewDeviceNeeded(requestType);
   int expiration = getAutheIDClientTimeout(requestType);

   rp::CreateRequest request;
   request.set_type(rp::DEVICE_KEY);
   request.mutable_device_key()->set_key_id(walletId);
   request.set_timeout_seconds(expiration);
   request.set_ra_pub_key(authKeys_.second.data(), authKeys_.second.size());

   request.set_title(action.toStdString() + "\nWallet ID:" + walletId);
   request.set_email(email_);
   request.mutable_device_key()->set_use_new_devices(newDevice);

   switch (requestType) {
   case ActivateWallet:
      request.mutable_device_key()->set_register_key(rp::CreateRequest::DeviceKey::REPLACE);
      break;
   case DeactivateWallet:
      request.mutable_device_key()->set_register_key(rp::CreateRequest::DeviceKey::CLEAR);
      break;
   case ActivateWalletNewDevice:
      request.mutable_device_key()->set_register_key(rp::CreateRequest::DeviceKey::ADD);
      break;
   case DeactivateWalletDevice:
      // No need to update anything, the server will sync it as needed to known device list
      request.mutable_device_key()->set_register_key(rp::CreateRequest::DeviceKey::KEEP);
      break;
   default:
      request.mutable_device_key()->set_register_key(rp::CreateRequest::DeviceKey::KEEP);
      break;
   }

   for (const std::string& knownDeviceId : knownDeviceIds) {
      request.mutable_device_key()->add_known_device_ids(knownDeviceId);
   }

   createCreateRequest(request.SerializeAsString(), expiration);
}

void AutheIDClient::authenticate(const std::string& email)
{
   requestAuth(email);
}

void AutheIDClient::requestAuth(const std::string& email)
{
   cancel();
   email_ = email;
   resultAuth_ = true;

   rp::CreateRequest request;
   auto signRequest = request.mutable_signature();
   signRequest->set_serialization(rp::SERIALIZATION_PROTOBUF);

   int expiration = getAutheIDClientTimeout(Unknown);

   request.set_title("Terminal Login");
   request.set_type(rp::AUTHENTICATION);
   request.set_ra_pub_key(authKeys_.second.data(), authKeys_.second.size());
   request.set_email(email);
   request.set_timeout_seconds(expiration);

   createCreateRequest(request.SerializeAsString(), expiration);
}

void AutheIDClient::sign(const BinaryData &data, const std::string &email
   , const QString &title, const QString &description, int expiration)
{
   cancel();
   email_ = email;

   rp::CreateRequest request;
   auto signRequest = request.mutable_signature();
   signRequest->set_serialization(rp::SERIALIZATION_PROTOBUF);
   signRequest->set_invisible_data(data.toBinStr());

   request.set_type(rp::SIGNATURE);
   request.set_timeout_seconds(expiration);
   request.set_ra_pub_key(authKeys_.second.data(), authKeys_.second.size());

   request.set_title(title.toStdString());
   request.set_description(description.toStdString());
   request.set_email(email);

   createCreateRequest(request.SerializeAsString(), expiration);
}

void AutheIDClient::cancel()
{
   if (requestId_.empty()) {
      return;
   }

   QNetworkRequest request = getRequest(fmt::format("{}/{}/cancel", baseUrl_, requestId_));

   QNetworkReply *reply = connectionManager_->GetNAM()->post(request, QByteArray());
   processNetworkReply(reply, kNetworkTimeoutSeconds, {});

   requestId_.clear();
}

void AutheIDClient::processCreateReply(const QByteArray &payload, int expiration)
{
   rp::CreateResponse response;
   if (!response.ParseFromArray(payload.data(), payload.size())) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(tr("Invalid create reply"));
      return;
   }

   requestId_ = response.request_id();

   QNetworkRequest request = getRequest(fmt::format("{}/{}", baseUrl_, requestId_));

   QNetworkReply *reply = connectionManager_->GetNAM()->get(request);
   processNetworkReply(reply, expiration, [this] (const Result &result) {
      if (!result.success) {
         emit failed(tr("Auth eID faild: %1").arg(QString::fromStdString(result.errorMsg)));
         return;
      }

      processResultReply(result.payload);
   });
}

void AutheIDClient::processResultReply(const QByteArray &payload)
{
   rp::GetResultResponse reply;
   if (!reply.ParseFromArray(payload.data(), payload.size())) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(tr("Invalid result reply"));
      return;
   }

   requestId_.clear();

   if (reply.has_signature()) {
      processSignatureReply(reply.signature());
      return;
   }

   if (resultAuth_)
   {
       std::string jwtToken = reply.authentication().jwt();
       if (!jwtToken.empty())
       {
            emit authSuccess(jwtToken);
       }
       else
       {
            emit failed(tr("Not authenticated"));
       }
       return;
   }

   if (reply.enc_secure_result().empty() || reply.device_id().empty()) {
      emit failed(tr("Cancelled"));
      return;
   }

   autheid::SecureBytes secureReplyData = autheid::decryptData(reply.enc_secure_result().data()
      , reply.enc_secure_result().size(), authKeys_.first);
   if (secureReplyData.empty()) {
      emit failed(tr("Decrypt failed"));
      return;
   }

   rp::GetResultResponse::SecureResult secureReply;
   if (!secureReply.ParseFromArray(secureReplyData.data(), int(secureReplyData.size()))) {
      emit failed(tr("Invalid secure reply"));
      return;
   }

   const std::string &deviceKey = secureReply.device_key();

   if (deviceKey.size() != kKeySize) {
      emit failed(tr("Invalid key size"));
      return;
   }

   std::string encKey = email_
      + kSeparatorSymbol + reply.device_id()
      + kSeparatorSymbol + reply.device_name();

   emit succeeded(encKey, SecureBinaryData(deviceKey));
}

void AutheIDClient::processNetworkReply(QNetworkReply *reply, int timeoutSeconds, const AutheIDClient::ResultCallback &callback)
{
   // Use this as context because AutheIDClient might be already destroyed when reply is finished!
   connect(reply, &QNetworkReply::finished, this, [reply, callback] {
      QByteArray payload = reply->readAll();

      Result result;

      if (reply->error()) {
         rp::Error error;
         // Auth eID will send rp::Error
         if (!payload.isEmpty() && error.ParseFromArray(payload.data(), payload.size())) {
            result.errorMsg = fmt::format("Auth eID failed: {}", error.message());
         } else {
            result.errorMsg = fmt::format("Auth eID failed: {}", reply->errorString().toStdString());
         }
         if (callback) {
            callback(result);
         }
         return;
      }

      result.success = true;
      result.payload = payload;
      if (callback) {
         callback(result);
      }
   });

   QTimer::singleShot(timeoutSeconds * 1000, reply, [reply] {
      // This would call finished slot and that would call callback if that is still needed
      reply->abort();
   });

   connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

QString AutheIDClient::getAutheIDClientRequestText(RequestType requestType)
{
   switch (requestType) {
   case ActivateWallet:
      return tr("Activate Auth eID Signing");
   case DeactivateWallet:
      return tr("Deactivate wallet");
   case SignWallet:
      return tr("Sign transaction");
   case BackupWallet:
      return tr("Backup wallet");
   case ActivateWalletOldDevice:
      return tr("Activate wallet (existing device)");
   case ActivateWalletNewDevice:
      return tr("Activate wallet (new device)");
   case DeactivateWalletDevice:
      return tr("Deactivate wallet device");
   case VerifyWalletKey:
      return tr("Confirm Auth eID Signing");
   case ActivateOTP:
      return tr("Activate OTP");
   case SettlementTransaction:
      return tr("Sign transaction");
   default:
      throw std::logic_error("Invalid AutheIDClient::RequestType value");
   }
}

bool AutheIDClient::isAutheIDClientNewDeviceNeeded(RequestType requestType)
{
   switch (requestType) {
   case ActivateWallet:
   case ActivateWalletNewDevice:
   case ActivateOTP:
      return true;
   default:
      return false;
   }
}

int AutheIDClient::getAutheIDClientTimeout(RequestType requestType)
{
   switch (requestType) {
   case SettlementTransaction:
      return 30;
   default:
      return 120;
   }
}

void AutheIDClient::processSignatureReply(const autheid::rp::GetResultResponse_SignatureResult &reply)
{
   if (reply.signature_data().empty() || reply.sign().empty()) {
      emit failed(tr("Missing mandatory signature data in reply"));
      return;
   }
   if (reply.serialization() != rp::SERIALIZATION_PROTOBUF) {
      emit failed(tr("Invalid signature serialization type"));
      return;
   }
   rp::GetResultResponse::SignatureResult::SignatureData sigData;
   if (!sigData.ParseFromString(reply.signature_data())) {
      emit failed(tr("Failed to parse signature data"));
      return;
   }

   autheid::PublicKey pubKey(reply.user_pub_key().begin(), reply.user_pub_key().end());
   if (!autheid::verifyData(reply.signature_data().data(), reply.signature_data().size(),
         reply.sign().data(), reply.sign().size(), pubKey)) {
      emit failed(tr("Signature validation failed"));
      return;
   }
   emit signSuccess(reply.signature_data(), sigData.invisible_data(), reply.sign());
}
