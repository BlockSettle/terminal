#include "AutheIDClient.h"

#include <google/protobuf/util/json_util.h>
#include <spdlog/spdlog.h>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <botan/x509cert.h>
#include <botan/data_src.h>
#include <botan/pubkey.h>
#include <botan/ocsp.h>
#include "rp.pb.h"

using namespace autheid;

namespace
{
   static_assert(int(AutheIDClient::Serialization::Json) == int(rp::SERIALIZATION_JSON), "please fix AutheIDClient::Serialization");
   static_assert(int(AutheIDClient::Serialization::Protobuf) == int(rp::SERIALIZATION_PROTOBUF), "please fix AutheIDClient::Serialization");

   const int kNetworkTimeoutSeconds = 10;
   const int kReplyTimeoutSeconds = 3;

   const int kKeySize = 32;

   static const char kSeparatorSymbol = ':';

   const auto ContentTypeHeader = "Content-Type";
   const auto AcceptHeader = "Accept";
   const auto AuthorizationHeader = "Authorization";

   const auto ProtobufMimeType = "application/protobuf";

   const auto kServerAddrLive = "https://api.autheid.com/v1/requests";
   const auto kAuthorizationKeyLive = "Bearer live_17ec2nlP5NzHWkEAQUwVpqhN63fiyDPWGc5Z3ZQ8npaf";

   const auto kServerAddrTest = "https://api.staging.autheid.com/v1/requests";
   const auto kAuthorizationKeyTest = "Bearer live_opnKv0PyeML0WvYm66ka2k29qPPoDjS3rzw13bRJzITY";

   const char AuthEidProdRootCert[] =
      "-----BEGIN CERTIFICATE-----"
      "MIICiDCCAg6gAwIBAgIVAOb2okkDBWR/iW2RuT8nYjpNplTqMAoGCCqGSM49BAMD"
      "MGUxCzAJBgNVBAYTAlNFMRwwGgYDVQQKExNBdXRoZW50aWNhdGUgZUlEIEFCMRow"
      "GAYDVQQLExFJbmZyYXN0cnVjdHVyZSBDQTEcMBoGA1UEAxMTQXV0aCBlSUQgUm9v"
      "dCBDQSB2MTAiGA8yMDE5MDUyMjAwMDAwMFoYDzIwMzkwNTIyMDAwMDAwWjBlMQsw"
      "CQYDVQQGEwJTRTEcMBoGA1UEChMTQXV0aGVudGljYXRlIGVJRCBBQjEaMBgGA1UE"
      "CxMRSW5mcmFzdHJ1Y3R1cmUgQ0ExHDAaBgNVBAMTE0F1dGggZUlEIFJvb3QgQ0Eg"
      "djEwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAR2kBiv/YrCKa/LfEBAIvjCmonftbeq"
      "wIjlTMFnVXTZTBTOMssMOeCByJYTv0ghR4g7BNTeYaCcriMA35UtlKeF6jiTnoQc"
      "6mGU2b19HwSyvtmIhlINeeCu5HfxLvHDFsijejB4MB0GA1UdDgQWBBQlqiF6NhIs"
      "6V7c/FEOvUCONlG2mDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAV"
      "BgNVHSAEDjAMMAoGCCqFcIIEAQEBMB8GA1UdIwQYMBaAFCWqIXo2EizpXtz8UQ69"
      "QI42UbaYMAoGCCqGSM49BAMDA2gAMGUCMCPwsYvJzRIRLvGyAVlbLj+E6BnvAfBb"
      "5+iNbo1RXrhTdIMb/oqcyEJbNbSSrrYYUwIxALTrwUrCe7t33RZOVVtuV+ZjiAm/"
      "j7izMBc1rGipMTl4L8vVqqmtz+2vXilxSb7dtA=="
      "-----END CERTIFICATE-----";

   const char AuthEidTestRootCert[] =
      "-----BEGIN CERTIFICATE-----"
      "MIICpzCCAi2gAwIBAgIVANCWAAe4NB0AbzvcOMzVt/eXvJOEMAoGCCqGSM49BAMD"
      "MHQxCzAJBgNVBAYTAlNFMSEwHwYDVQQKExhUZXN0IEF1dGhlbnRpY2F0ZSBlSUQg"
      "QUIxHzAdBgNVBAsTFlRlc3QgSW5mcmFzdHJ1Y3R1cmUgQ0ExITAfBgNVBAMTGFRl"
      "c3QgQXV0aCBlSUQgUm9vdCBDQSB2MTAiGA8yMDE5MDUyMTAwMDAwMFoYDzIwMzkw"
      "NTIxMDAwMDAwWjB0MQswCQYDVQQGEwJTRTEhMB8GA1UEChMYVGVzdCBBdXRoZW50"
      "aWNhdGUgZUlEIEFCMR8wHQYDVQQLExZUZXN0IEluZnJhc3RydWN0dXJlIENBMSEw"
      "HwYDVQQDExhUZXN0IEF1dGggZUlEIFJvb3QgQ0EgdjEwdjAQBgcqhkjOPQIBBgUr"
      "gQQAIgNiAATttsFmSGlfGgeBWCO+G4j+LaheRZksckdz0ks2DrUz+eBLAdY5neE1"
      "uwvidGXuebR4c3Kr7TbBZaQbmIHEd3kUTQ4paqKWQKgck5WJNYPm2wgpS7co8Fjk"
      "jaFG4Mu9QZujezB5MB0GA1UdDgQWBBRaWiMIx4yz8dBVsBVyR0qL9wVSejAOBgNV"
      "HQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAWBgNVHSAEDzANMAsGCSqFcL2E"
      "PwEBATAfBgNVHSMEGDAWgBRaWiMIx4yz8dBVsBVyR0qL9wVSejAKBggqhkjOPQQD"
      "AwNoADBlAjBaJB4PI9hFk0teclJEPWfXUt1CovrWY3nWlOkyl+usJFkgJZH1yIFI"
      "uYVsbv9LK7QCMQCc9MQEu9tZLzVCcucBy2tbNYF1BPUE4Z51gohpiBTxbosqy9L2"
      "61lZsHVbH/v/HtY="
      "-----END CERTIFICATE-----";

   const char AuthEidCertHash[] = "EMSA1(SHA-256)";

   const char AuthEidUniqueUserIdField[] = "X520.CommonName";

   QNetworkRequest getRequest(const char *url, const char *apiKey)
   {
      QNetworkRequest request;
      request.setUrl(QUrl(QString::fromLatin1(url)));
      request.setRawHeader(ContentTypeHeader, ProtobufMimeType);
      request.setRawHeader(AcceptHeader, ProtobufMimeType);
      request.setRawHeader(AuthorizationHeader, QByteArray(apiKey));
      return request;
   }

} // namespace

QString AutheIDClient::errorString(AutheIDClient::ErrorType error)
{
   switch (error) {
   case NoError:
      return tr("No error");
   case CreateError:
      return tr("Invalid create reply");
   case DecodeError:
      return tr("Invalid result reply");
   case DecryptError:
      return tr("Decrypt failed");
   case InvalidSecureReplyError:
      return tr("Invalid secure relply");
   case InvalidKeySizeError:
      return tr("Invalid key size");
   case MissingSignatuteError:
      return tr("Missing mandatory signature data in reply");
   case SerializationSignatureError:
      return tr("Invalid signature serialization type");
   case ParseSignatureError:
      return tr("Failed to parse signature data");
   case Timeout:
      return tr("Operation time exceeded");
   case Cancelled:
      return tr("Operation cancelled");
   case NotAuthenticated:
      return tr("Not authenticated");
   case ServerError:
      return tr("Server error");
   case NetworkError:
      return tr("Network error");
   }

   return tr("Unknown error");
}

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

AutheIDClient::SignVerifyStatus AutheIDClient::verifySignature(const SignResult &signResult, AuthEidEnv env)
{
   try {
      // DER encoding
      Botan::DataSource_Memory clientRaw(signResult.certificateClient.getPtr()
         , signResult.certificateClient.getSize());
      Botan::X509_Certificate client(clientRaw);

      // DER encoding
      Botan::DataSource_Memory issuerRaw(signResult.certificateIssuer.getPtr()
         , signResult.certificateIssuer.getSize());
      Botan::X509_Certificate issuer(issuerRaw);

      // PEM encoding
      Botan::DataSource_Memory rootRaw(env == AuthEidEnv::Prod ? AuthEidProdRootCert : AuthEidTestRootCert);
      Botan::X509_Certificate root(rootRaw);

      auto clientPubKey = client.load_subject_public_key();

      Botan::PK_Verifier verifier(*clientPubKey, AuthEidCertHash, Botan::DER_SEQUENCE);
      verifier.update(signResult.data.getPtr(), signResult.data.getSize());
      bool result = verifier.check_signature(signResult.sign.getPtr(), signResult.sign.getSize());
      if (!result) {
         return SignVerifyStatus::failed("invalid signature");
      }

      rp::GetResultResponse::SignatureResult::SignatureData sigData;

      switch (signResult.serialization) {
         case Serialization::Protobuf: {
            bool result = sigData.ParseFromArray(signResult.data.getPtr(), int(signResult.data.getSize()));
            if (!result) {
               return SignVerifyStatus::failed("Protobuf deserialization failed");
            }
            break;
         }

         case Serialization::Json: {
            auto status = google::protobuf::util::JsonStringToMessage(signResult.data.toBinStr(), &sigData);
            if (!status.ok()) {
               return SignVerifyStatus::failed("JSON deserialization failed");
            }
            break;
         }
      }

      auto signTimestamp = std::chrono::system_clock::from_time_t(sigData.timestamp_finished());

      // Verify that issuer's certificate is valid
      result = issuer.check_signature(*root.load_subject_public_key());
      if (!result) {
         return SignVerifyStatus::failed("invalid issuer's certificate");
      }

      // Verify that client's certificate is valid
      result = client.check_signature(*issuer.load_subject_public_key());
      if (!result) {
         return SignVerifyStatus::failed("invalid client's certificate");
      }

      Botan::OCSP::Response ocsp(signResult.ocspResponse.getPtr(), signResult.ocspResponse.getSize());
      Botan::Certificate_Status_Code verifyResult = ocsp.status_for(issuer, client, signTimestamp);
      if (verifyResult != Botan::Certificate_Status_Code::OCSP_RESPONSE_GOOD) {
         return SignVerifyStatus::failed("invalid OCSP response");
      }

      std::string certUniqueUserId = client.subject_dn().get_first_attribute(AuthEidUniqueUserIdField);
      if (certUniqueUserId.empty()) {
         return SignVerifyStatus::failed("invalid empty unique user Id in the certificate");
      }

      SignVerifyStatus status;
      status.valid = true;

      status.uniqueUserId = certUniqueUserId;

      status.email = sigData.email();
      status.rpName = sigData.rp_name();
      status.title = sigData.title();
      status.description = sigData.description();
      status.finished = signTimestamp;
      status.invisibleData = sigData.invisible_data();
      return status;
   } catch (const std::exception &e) {
      return SignVerifyStatus::failed(fmt::format("signature verification failed: {}", e.what()));
   }
}

AutheIDClient::AutheIDClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<QNetworkAccessManager> &nam
   , const AuthKeys &authKeys
   , bool autheidTestEnv
   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , nam_(nam)
   , authKeys_(authKeys)
   , resultAuth_(false)
{
   if (autheidTestEnv) {
      baseUrl_ = kServerAddrTest;
      apiKey_ = kAuthorizationKeyTest;
   } else {
      baseUrl_ = kServerAddrLive;
      apiKey_ = kAuthorizationKeyLive;
   }
}

AutheIDClient::~AutheIDClient()
{
   cancel();
}

void AutheIDClient::createCreateRequest(const std::string &payload, int expiration, bool autoRequestResult)
{
   QNetworkRequest request = getRequest(baseUrl_, apiKey_);

   QNetworkReply *reply = nam_->post(request, QByteArray::fromStdString(payload));
   processNetworkReply(reply, kNetworkTimeoutSeconds, [this, expiration, autoRequestResult] (const Result &result) {
      if (result.networkError != QNetworkReply::NoError) {
         emit failed(result.authError != NoError ? result.authError : NetworkError);
         return;
      }

      processCreateReply(result.payload, expiration, autoRequestResult);
   });
}

void AutheIDClient::start(RequestType requestType, const std::string &email
   , const std::string &walletId, const std::vector<std::string> &knownDeviceIds, int expiration)
{
   cancel();

   email_ = email;

   QString action = getAutheIDClientRequestText(requestType);
   bool newDevice = isAutheIDClientNewDeviceNeeded(requestType);

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

   createCreateRequest(request.SerializeAsString(), expiration, true);
}

void AutheIDClient::authenticate(const std::string &email, int expiration, bool autoRequestResult)
{
   cancel();
   email_ = email;
   resultAuth_ = true;

   rp::CreateRequest request;
   auto signRequest = request.mutable_signature();
   signRequest->set_serialization(rp::SERIALIZATION_PROTOBUF);

   request.set_title("Terminal Login");
   request.set_type(rp::AUTHENTICATION);
   request.set_ra_pub_key(authKeys_.second.data(), authKeys_.second.size());
   request.set_email(email);
   request.set_timeout_seconds(expiration);

   createCreateRequest(request.SerializeAsString(), expiration, autoRequestResult);
}

void AutheIDClient::sign(const SignRequest &request)
{
   assert(!request.email.empty());
   assert(!request.title.empty());

   cancel();
   email_ = request.email;

   rp::CreateRequest createRequest;
   auto d = createRequest.mutable_signature();
   d->set_serialization(rp::Serialization(request.serialization));
   d->set_invisible_data(request.invisibleData.toBinStr());

   createRequest.set_type(rp::SIGNATURE);
   createRequest.set_timeout_seconds(request.expiration);
   createRequest.set_ra_pub_key(authKeys_.second.data(), authKeys_.second.size());

   createRequest.set_title(request.title);
   createRequest.set_description(request.description);
   createRequest.set_email(request.email);

   createCreateRequest(createRequest.SerializeAsString(), request.expiration, true);

   // Make a copy to check sign result later
   signRequest_ = request;
}

void AutheIDClient::cancel()
{
   if (requestId_.empty()) {
      return;
   }

   QNetworkRequest request = getRequest(fmt::format("{}/{}/cancel", baseUrl_, requestId_).c_str(), apiKey_);

   QNetworkReply *reply = nam_->post(request, QByteArray());
   processNetworkReply(reply, kNetworkTimeoutSeconds, {});

   requestId_.clear();
}

void AutheIDClient::requestResult()
{
   QNetworkRequest request = getRequest(fmt::format("{}/{}", baseUrl_, requestId_).c_str(), apiKey_);

   QNetworkReply *reply = nam_->get(request);
   processNetworkReply(reply, expiration_, [this] (const Result &result) {
      processResultReply(result.payload);
   });
}

void AutheIDClient::processCreateReply(const QByteArray &payload, int expiration, bool autoRequestResult)
{
   rp::CreateResponse response;
   if (!response.ParseFromArray(payload.data(), payload.size())) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(ErrorType::CreateError);
      return;
   }

   requestId_ = response.request_id();
   expiration_ = expiration;
   emit createRequestDone();
   if (autoRequestResult) {
      requestResult();
   }
}

void AutheIDClient::processResultReply(const QByteArray &payload)
{
   rp::GetResultResponse reply;
   if (!reply.ParseFromArray(payload.data(), payload.size())) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(ErrorType::DecodeError);
      return;
   }

   requestId_.clear();

   if (reply.has_signature()) {
      processSignatureReply(reply.signature());
      return;
   }

   if (reply.status() == rp::RP_CANCELLED || reply.status() == rp::USER_CANCELLED) {
      emit userCancelled();
      return;
   }

   if (reply.status() == rp::TIMEOUT) {
      emit failed(ErrorType::Timeout);
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
         emit failed(ErrorType::NotAuthenticated);
      }
      return;
   }

   if (reply.device_key_enc().empty() || reply.device_id().empty()) {
      emit failed(ErrorType::Cancelled);
      return;
   }

   autheid::SecureBytes secureReplyData = autheid::decryptData(reply.device_key_enc().data()
      , reply.device_key_enc().size(), authKeys_.first);
   if (secureReplyData.empty()) {
      emit failed(ErrorType::DecryptError);
      return;
   }

   rp::GetResultResponse::DeviceKeyResult secureReply;
   if (!secureReply.ParseFromArray(secureReplyData.data(), int(secureReplyData.size()))) {
      emit failed(ErrorType::InvalidSecureReplyError);
      return;
   }

   const std::string &deviceKey = secureReply.device_key();

   if (deviceKey.size() != kKeySize) {
      emit failed(ErrorType::InvalidKeySizeError);
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
   connect(reply, &QNetworkReply::finished, this, [this, reply, callback] {
      QByteArray payload = reply->readAll();

      Result result;
      result.networkError = reply->error();

      if (reply->error() == QNetworkReply::OperationCanceledError) {
         return;
      }

      if (reply->error() == QNetworkReply::TimeoutError) {
         emit failed(ErrorType::NetworkError);
         return;
      }

      if (reply->error()) {
         rp::Error error;
         // Auth eID will send rp::Error
         if (payload.isEmpty() || !error.ParseFromArray(payload.data(), payload.size())) {
            logger_->error("AuthEid failed: network error {}({})", reply->error(), reply->errorString().toStdString());
            emit failed(ErrorType::ServerError);
            return;
         }

         logger_->error("AuthEid server error: {}", error.message());
         emit failed(ErrorType::ServerError);
         return;
      }

      result.payload = payload;
      if (callback) {
         callback(result);
      }
   });

   // This would call finished slot and that would call callback if that is still needed
   // Normally singleshot is not called since timeout triggered by server response
   // Extra 3 seconds added to singleshot timer to be sure that the timeout server response will be received
   QTimer::singleShot(timeoutSeconds * 1000 + kReplyTimeoutSeconds * 1000, reply, [reply] {
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

void AutheIDClient::processSignatureReply(const autheid::rp::GetResultResponse_SignatureResult &reply)
{
   if (reply.signature_data().empty() || reply.sign().empty()) {
      emit failed(ErrorType::MissingSignatuteError);
      return;
   }

   rp::GetResultResponse::SignatureResult::SignatureData sigData;

   switch (reply.serialization()) {
      case rp::SERIALIZATION_PROTOBUF: {
         bool result = sigData.ParseFromString(reply.signature_data());
         if (!result) {
            SPDLOG_LOGGER_ERROR(logger_, "parsing signature protobuf from AuthEid failed");
            emit failed(ErrorType::ParseSignatureError);
            return;
         }
         break;
      }

      case rp::SERIALIZATION_JSON: {
         auto status = google::protobuf::util::JsonStringToMessage(reply.signature_data(), &sigData);
         if (!status.ok()) {
            SPDLOG_LOGGER_ERROR(logger_, "parsing signature json from AuthEid failed");
            emit failed(ErrorType::ParseSignatureError);
            return;
         }
         break;
      }

      default:
         SPDLOG_LOGGER_ERROR(logger_, "unknown serialization from AuthEid");
         emit failed(ErrorType::SerializationSignatureError);
         return;
   }

   // Make sure we got in the response what was requested (for security reasons)
   if (signRequest_.title != sigData.title()
       || signRequest_.email != sigData.email()
       || signRequest_.description != sigData.description()
       || signRequest_.invisibleData.toBinStr() != sigData.invisible_data())
   {
      SPDLOG_LOGGER_ERROR(logger_, "invalid response from AuthEid detected");
      emit failed(ErrorType::SerializationSignatureError);
      return;
   }

   // We could verify certificate and signature here if that is needed
   // Example: https://github.com/autheid/AuthSamples/blob/master/cpp/main.cpp#L116

   SignResult result;
   result.serialization = Serialization(reply.serialization());
   result.data = reply.signature_data();
   result.sign = reply.sign();
   result.certificateClient = reply.certificate_client();
   result.certificateIssuer = reply.certificate_issuer();
   result.ocspResponse = reply.ocsp_response();

   emit signSuccess(result);
}
