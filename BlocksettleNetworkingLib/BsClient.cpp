#include "BsClient.h"

#include <QTimer>
#include "ProtobufUtils.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "bs_proxy.pb.h"

using namespace Blocksettle::Communication::Proxy;

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
      , const std::string& srvAddrPort, const std::shared_ptr<std::promise<bool>> &prompt)
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

   sendRequest(&request, getDefaultAutheidAuthTimeout(), [this] {
      emit getLoginResultDone(AutheIDClient::NetworkError);
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

std::chrono::seconds BsClient::getDefaultAutheidAuthTimeout()
{
   return std::chrono::seconds(60);
}

void BsClient::OnDataReceived(const std::string &data)
{
   auto response = std::make_shared<Response>();
   bool result = response->ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse from BS proxy");
      return;
   }

   if (response->data_case() != Response::kCeler) {
      SPDLOG_LOGGER_DEBUG(logger_, "bs recv: {}", ProtobufUtils::toJsonCompact(*response));
   }

   QMetaObject::invokeMethod(this, [this, response] {
      if (response->request_id() != 0) {
         auto it = activeRequests_.find(response->request_id());
         if (it == activeRequests_.end()) {
            SPDLOG_LOGGER_ERROR(logger_, "discard late response from BsProxy (requestId: {})", response->request_id());
            return;
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
   , FailedCallback failedCb)
{
   const int64_t requestId = newRequestId();
   ActiveRequest activeRequest;
   activeRequest.failedCb = std::move(failedCb);
   activeRequests_.emplace(requestId, std::move(activeRequest));

   QTimer::singleShot(timeout, this, [this, requestId] {
      auto it = activeRequests_.find(requestId);
      if (it == activeRequests_.end()) {
         return;
      }

      it->second.failedCb();
      activeRequests_.erase(it);
   });

   request->set_request_id(requestId);
   sendMessage(request);
}

void BsClient::sendMessage(Request *request)
{
   if (request->data_case() != Request::kCeler) {
      SPDLOG_LOGGER_DEBUG(logger_, "bs send: {}", ProtobufUtils::toJsonCompact(*request));
   }

   connection_->send(request->SerializeAsString());
}

void BsClient::processStartLogin(const Response_StartLogin &response)
{
   emit startLoginDone(AutheIDClient::ErrorType(response.error_code()));
}

void BsClient::processGetLoginResult(const Response_GetLoginResult &response)
{
   emit getLoginResultDone(AutheIDClient::ErrorType(response.error_code()));
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

int64_t BsClient::newRequestId()
{
   lastRequestId_ += 1;
   return lastRequestId_;
}
