#include "BsClient.h"
#include "FutureValue.h"

#include <QTimer>
#include "Address.h"
#include "ProtobufUtils.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "bs_proxy_terminal.pb.h"
#include "bs_proxy_terminal_pb.pb.h"

using namespace Blocksettle::Communication;
using namespace Blocksettle::Communication::ProxyTerminal;

BsClient::BsClient(const std::shared_ptr<spdlog::logger> &logger
   , const BsClientParams &params, QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , params_(params)
{
   ZmqBIP15XDataConnectionParams zmqBipParams;
   zmqBipParams.ephemeralPeers = true;
   connection_ = std::make_unique<ZmqBIP15XDataConnection>(logger, zmqBipParams);

   connection_->setCBs([this](const std::string &oldKey, const std::string &newKey
      , const std::string& srvAddrPort, const std::shared_ptr<FutureValue<bool>> &prompt)
   {
      BsClientParams::NewKey d;
      d.oldKey = oldKey;
      d.newKey = newKey;
      d.prompt = prompt;
      params_.newServerKeyCallback(d);
   });

   // This should not ever fail
   bool result = connection_->openConnection(params_.connectAddress, std::to_string(params_.connectPort), this);
   assert(result);
}

BsClient::~BsClient()
{
   // Stop receiving events from DataConnectionListener before BsClient is partially destroyed
   connection_.reset();
}

void BsClient::startLogin(const std::string &email)
{
   Request request;
   auto d = request.mutable_start_login();
   d->set_email(email);

   sendRequest(&request, std::chrono::seconds(10), [this] {
      emit startLoginDone(AutheIDClient::NetworkError);
   });
}

void BsClient::sendPbMessage(std::string data)
{
   Request request;
   auto d = request.mutable_proxy_pb();
   d->set_data(std::move(data));
   sendMessage(&request);
}

void BsClient::sendUnsignedPayin(const std::string& settlementId, const BinaryData& unsignedPayin, const BinaryData& unsignedTxId)
{
   ProxyTerminalPb::Request request;

   auto data = request.mutable_unsigned_payin();
   data->set_settlement_id(settlementId);
   data->set_unsigned_payin(unsignedPayin.toBinStr());
   data->set_unsigned_payin_id(unsignedTxId.toBinStr());

   sendPbMessage(request.SerializeAsString());
}

void BsClient::sendSignedPayin(const std::string& settlementId, const BinaryData& signedPayin)
{
   ProxyTerminalPb::Request request;

   auto data = request.mutable_signed_payin();
   data->set_settlement_id(settlementId);
   data->set_signed_payin(signedPayin.toBinStr());

   sendPbMessage(request.SerializeAsString());
}

void BsClient::sendSignedPayout(const std::string& settlementId, const BinaryData& signedPayout)
{
   ProxyTerminalPb::Request request;

   auto data = request.mutable_signed_payout();
   data->set_settlement_id(settlementId);
   data->set_signed_payout(signedPayout.toBinStr());

   sendPbMessage(request.SerializeAsString());
}

void BsClient::cancelLogin()
{
   Request request;
   request.mutable_cancel_login();
   sendMessage(&request);
}

void BsClient::getLoginResult()
{
   Request request;
   request.mutable_get_login_result();

   // Add some time to be able get timeout error from the server
   sendRequest(&request, autheidLoginTimeout() + std::chrono::seconds(3), [this] {
      emit getLoginResultDone(AutheIDClient::NetworkError, {});
   });
}

void BsClient::logout()
{
   Request request;
   request.mutable_logout();
   sendMessage(&request);
}

void BsClient::celerSend(CelerAPI::CelerMessageType messageType, const std::string &data)
{
   Request request;
   auto d = request.mutable_celer();
   d->set_message_type(int(messageType));
   d->set_data(data);
   sendMessage(&request);
}

void BsClient::signAddress(const SignAddressReq &req)
{
   assert(req.type != SignAddressReq::Unknown);

   Request request;
   auto d = request.mutable_start_sign_address();
   d->set_type(int(req.type));
   d->set_address(req.address.display());
   d->set_invisible_data(req.invisibleData.toBinStr());
   d->set_src_cc_token(req.srcCcToken);

   auto processCb = [this, req](const Response &response) {
      if (!response.has_start_sign_address()) {
         SPDLOG_LOGGER_ERROR(logger_, "unexpected response from BsProxy, expected StartSignAuthAddress response");
         req.failedCb(AutheIDClient::ServerError);
         return;
      }

      const auto &d = response.start_sign_address();
      if (d.error().error_code() != 0) {
         SPDLOG_LOGGER_ERROR(logger_, "signature request start failed on the server");
         req.failedCb(AutheIDClient::ErrorType(d.error().error_code()));
         return;
      }

      if (req.startedCb) {
         req.startedCb();
      }

      // Add some time to be able get timeout error from the server
      requestSignResult(autheidAuthAddressTimeout() + std::chrono::seconds(3), req.signedCb, req.failedCb);
   };

   auto timeoutCb = [failedCb = req.failedCb] {
      failedCb(AutheIDClient::NetworkError);
   };

   sendRequest(&request, std::chrono::seconds(10), std::move(timeoutCb), std::move(processCb));
}

// static
std::chrono::seconds BsClient::autheidLoginTimeout()
{
   return std::chrono::seconds(60);
}

// static
std::chrono::seconds BsClient::autheidAuthAddressTimeout()
{
   return std::chrono::seconds(30);
}

// static
std::chrono::seconds BsClient::autheidCcAddressTimeout()
{
   return std::chrono::seconds(90);
}

// static
std::string BsClient::requestTitleAuthAddr()
{
   return "Authentication Address";
}

// static
std::string BsClient::requestDescAuthAddr(const bs::Address &address)
{
   return fmt::format("Submit auth address for verification: {}", address.display());
}

// static
std::string BsClient::requestTitleCcAddr()
{
   return "Private Market Token";
}

// static
std::string BsClient::requestDescCcAddr(const bs::Address &address, const std::string &token)
{
   // We don't show address details here yet
   return token;
}

void BsClient::OnDataReceived(const std::string &data)
{
   auto response = std::make_shared<Response>();
   bool result = response->ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse response from BS proxy");
      return;
   }

   QMetaObject::invokeMethod(this, [this, response] {
      if (response->request_id() != 0) {
         auto it = activeRequests_.find(response->request_id());
         if (it == activeRequests_.end()) {
            SPDLOG_LOGGER_ERROR(logger_, "discard late response from BsProxy (requestId: {})", response->request_id());
            return;
         }

         if (it->second.processCb) {
            it->second.processCb(*response);
         }

         activeRequests_.erase(it);
      }

      switch (response->data_case()) {
         case Response::kStartLogin:
            processStartLogin(response->start_login());
            return;
         case Response::kGetLoginResult:
            processGetLoginResult(response->get_login_result());
            return;
         case Response::kCeler:
            processCeler(response->celer());
            return;
         case Response::kProxyPb:
            processProxyPb(response->proxy_pb());
            return;
         case Response::kStartSignAddress:
         case Response::kGetSignResult:
            // Will be handled from processCb
            return;
         case Response::DATA_NOT_SET:
            return;
      }

      SPDLOG_LOGGER_CRITICAL(logger_, "unknown response was detected!");
   });
}

void BsClient::OnConnected()
{
   emit connected();
}

void BsClient::OnDisconnected()
{
   emit disconnected();
}

void BsClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   SPDLOG_LOGGER_ERROR(logger_, "connection to bs proxy failed ({})", int(errorCode));

   emit connectionFailed();
}

void BsClient::sendRequest(Request *request, std::chrono::milliseconds timeout
   , TimeoutCb timeoutCb, ProcessCb processCb)
{
   const int64_t requestId = newRequestId();
   ActiveRequest activeRequest;
   activeRequest.processCb = std::move(processCb);
   activeRequest.timeoutCb = std::move(timeoutCb);
   activeRequests_.emplace(requestId, std::move(activeRequest));

   QTimer::singleShot(timeout, this, [this, requestId] {
      auto it = activeRequests_.find(requestId);
      if (it == activeRequests_.end()) {
         return;
      }

      // Erase iterator before calling callback!
      // Callback could be be blocking and iterator might become invalid after callback return.
      auto callback = std::move(it->second.timeoutCb);
      activeRequests_.erase(it);

      // Callback could be blocking
      callback();
   });

   request->set_request_id(requestId);
   sendMessage(request);
}

void BsClient::sendMessage(Request *request)
{
   connection_->send(request->SerializeAsString());
}

void BsClient::processStartLogin(const Response_StartLogin &response)
{
   emit startLoginDone(AutheIDClient::ErrorType(response.error().error_code()));
}

void BsClient::processGetLoginResult(const Response_GetLoginResult &response)
{
   emit getLoginResultDone(AutheIDClient::ErrorType(response.error().error_code()), response.celer_login());
}

void BsClient::processCeler(const Response_Celer &response)
{
   auto messageType = CelerAPI::CelerMessageType(response.message_type());
   if (!CelerAPI::isValidMessageType(messageType)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid celer msg type received: {}", int(messageType));
      return;
   }

   emit celerRecv(messageType, response.data());
}

void BsClient::processProxyPb(const Response_ProxyPb &response)
{
   emit processPbMessage(response.data());
}

void BsClient::requestSignResult(std::chrono::seconds timeout
   , const BsClient::SignedCb &signedCb, const BsClient::SignFailedCb &failedCb)
{
   auto successCbWrap = [this, signedCb, failedCb](const Response &response) {
      if (!response.has_get_sign_result()) {
         SPDLOG_LOGGER_ERROR(logger_, "unexpected response from BsProxy, expected GetSignResult response");
         failedCb(AutheIDClient::ServerError);
         return;
      }

      const auto &d = response.get_sign_result();
      if (d.error().error_code() != 0) {
         SPDLOG_LOGGER_ERROR(logger_, "getting sign response faild on the server");
         failedCb(AutheIDClient::ErrorType(d.error().error_code()));
         return;
      }

      AutheIDClient::SignResult result;
      result.serialization = AutheIDClient::Serialization::Protobuf;
      result.data = d.sign_data();
      result.sign = d.sign();
      result.certificateClient = d.certificate_client();
      result.certificateIssuer = d.certificate_issuer();
      result.ocspResponse = d.ocsp_response();
      signedCb(result);
   };

   auto failedCbWrap = [failedCb] {
      failedCb(AutheIDClient::NetworkError);
   };

   Request request;
   request.mutable_get_sign_result();
   sendRequest(&request, timeout, std::move(failedCbWrap), std::move(successCbWrap));
}

int64_t BsClient::newRequestId()
{
   lastRequestId_ += 1;
   return lastRequestId_;
}
