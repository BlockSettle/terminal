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
   startTimer(std::chrono::milliseconds(1000));

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

   bool result = connection_->openConnection(params_.connectAddress, std::to_string(params_.connectPort), this);
   if (!result) {
      throw std::invalid_argument(fmt::format("can't open BsClient connection to {}:{}"
         , params_.connectAddress, params_.connectPort));
   }
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
      emit startLoginDone(false);
   });
}

void BsClient::cancelLogin()
{
   Request request;
   request.mutable_cancel_login();

   sendRequest(&request, std::chrono::seconds(10), [this] {
      emit cancelLoginDone(false);
   });
}

void BsClient::getLoginResult()
{
   Request request;
   request.mutable_get_login_result();

   sendRequest(&request, std::chrono::seconds(60), [this] {
      emit getLoginResultDone(false);
   });
}

void BsClient::celerSend(CelerAPI::CelerMessageType messageType, const std::string &data)
{
   Request request;
   auto d = request.mutable_celer();
   d->set_message_type(int(messageType));
   d->set_data(data);
   sendMessage(&request);
}

void BsClient::timerEvent(QTimerEvent *event)
{
   const auto now = std::chrono::steady_clock::now();
   auto it = activeRequests_.begin();
   while (it != activeRequests_.end() && it->first < now) {
      auto activeRequest = std::move(it->second);
      it = activeRequests_.erase(it);

      auto itId = activeRequestIds_.find(activeRequest.requestId);
      if (itId != activeRequestIds_.end()) {
         activeRequestIds_.erase(itId);
         activeRequest.failedCb();
      }
   }
}

void BsClient::OnDataReceived(const std::string &data)
{
   auto response = std::make_shared<Response>();
   bool result = response->ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse from BS proxy");
      return;
   }

   SPDLOG_LOGGER_DEBUG(logger_, "bs recv: {}", ProtobufUtils::toJsonCompact(*response));

   QMetaObject::invokeMethod(this, [this, response] {
      if (response->request_id() != 0) {
         auto it = activeRequestIds_.find(response->request_id());
         if (it == activeRequestIds_.end()) {
            SPDLOG_LOGGER_ERROR(logger_, "discard late response from BsProxy (requestId: {})", response->request_id());
            return;
         }

         activeRequestIds_.erase(it);
      }

      switch (response->data_case()) {
         case Response::kStartLogin:
            processStartLogin(response->start_login());
            return;
         case Response::kCancelLogin:
            processCancelLogin(response->cancel_login());
            return;
         case Response::kGetLoginResult:
            processGetLoginResult(response->get_login_result());
            return;
         case Response::kLogout:
            processLogout(response->logout());
            return;
         case Response::kCeler:
            processCeler(response->celer());
            return;
         case Response::DATA_NOT_SET:
            return;
      }

      SPDLOG_LOGGER_CRITICAL(logger_, "FIXME: response was not processed!");
   });
}

void BsClient::OnConnected()
{
}

void BsClient::OnDisconnected()
{
}

void BsClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
}

void BsClient::sendRequest(Request *request, std::chrono::milliseconds timeout
   , FailedCallback failedCb)
{
   ActiveRequest activeRequest;
   activeRequest.requestId = newRequestId();
   activeRequest.failedCb = std::move(failedCb);

   activeRequestIds_.insert(activeRequest.requestId);
   activeRequests_.emplace(std::chrono::steady_clock::now() + timeout, std::move(activeRequest));

   request->set_request_id(activeRequest.requestId);
   sendMessage(request);
}

void BsClient::sendMessage(Request *request)
{
   SPDLOG_LOGGER_DEBUG(logger_, "bs send: {}", ProtobufUtils::toJsonCompact(*request));

   connection_->send(request->SerializeAsString());
}

void BsClient::processStartLogin(const Response_StartLogin &response)
{
   emit startLoginDone(response.success());
}

void BsClient::processCancelLogin(const Response_CancelLogin &response)
{
   emit cancelLoginDone(response.success());
}

void BsClient::processGetLoginResult(const Response_GetLoginResult &response)
{
   emit getLoginResultDone(response.success());
}

void BsClient::processLogout(const Response_Logout &response)
{
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
