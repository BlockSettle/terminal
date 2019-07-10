#include "BsClient.h"

#include <QTimer>
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

void BsClient::startLogin(const std::string &login)
{
   Request request;
   auto d = request.mutable_start_login();
   d->set_auth_id(login);

   sendRequest(&request, std::chrono::seconds(10), [this] {
      emit startLoginDone(false);
   });
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
            process(response->start_login());
            break;
         case Response::kCancelLogin:
            process(response->cancel_login());
            break;
         case Response::kGetLoginResult:
            process(response->get_login_result());
            break;
         case Response::kLogout:
            process(response->logout());
            break;
         case Response::DATA_NOT_SET:
            break;
      }
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

   request->set_request_id(activeRequest.requestId);
   connection_->send(request->SerializeAsString());

   activeRequestIds_.insert(activeRequest.requestId);
   activeRequests_.emplace(std::chrono::steady_clock::now() + timeout, std::move(activeRequest));
}

void BsClient::process(const Response_StartLogin &response)
{
   emit startLoginDone(response.success());
}

void BsClient::process(const Response_CancelLogin &response)
{

}

void BsClient::process(const Response_GetLoginResult &response)
{

}

void BsClient::process(const Response_Logout &response)
{

}

int64_t BsClient::newRequestId()
{
   lastRequestId_ += 1;
   return lastRequestId_;
}
