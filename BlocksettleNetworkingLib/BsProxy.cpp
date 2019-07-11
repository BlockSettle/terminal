#include "BsProxy.h"

#include <QNetworkAccessManager>
#include "AutheIDClient.h"
#include "LoggerHelpers.h"
#include "StringUtils.h"
#include "ZMQ_BIP15X_ServerConnection.h"
#include "bs_proxy.pb.h"
#include "rp.pb.h"

using namespace Blocksettle::Communication::Proxy;
using namespace autheid;

namespace {

   ZmqBIP15XPeers emptyTrustedClientsCallback()
   {
      return ZmqBIP15XPeers();
   }

} // namespace

class BsProxyListener : public ServerConnectionListener
{
public:
   void OnDataFromClient(const std::string& clientId, const std::string& data) override
   {
      proxy_->onProxyDataFromClient(clientId, data);
   }

   void OnClientConnected(const std::string& clientId) override
   {
      proxy_->onProxyClientConnected(clientId);
   }

   void OnClientDisconnected(const std::string& clientId) override
   {
      proxy_->onProxyClientDisconnected(clientId);
   }

   BsProxy *proxy_{};
};

BsProxy::BsProxy(const std::shared_ptr<spdlog::logger> &logger, const BsProxyParams &params)
   : logger_(logger)
   , params_(params)
{
   serverListener_ = std::make_unique<BsProxyListener>();
   serverListener_->proxy_ = this;

   server_ = std::make_unique<ZmqBIP15XServerConnection>(logger
      , params.context, &emptyTrustedClientsCallback, false, params.ownKeyFileDir, params.ownKeyFileName);

   bool result = server_->BindConnection(params.listenAddress, std::to_string(params.listenPort), serverListener_.get());
   if (!result) {
      throw std::runtime_error(fmt::format("can't bind to {}:{}", params.listenAddress, params.listenPort));
   }

   nam_ = std::make_shared<QNetworkAccessManager>(this);
}

BsProxy::~BsProxy() = default;

void BsProxy::onProxyDataFromClient(const std::string &clientId, const std::string &data)
{
   auto request = std::make_shared<Request>();
   bool result = request->ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse request from {}", bs::toHex(clientId));
   }

   QMetaObject::invokeMethod(this, [this, clientId, request] {
      auto client = findClient(clientId);
      BS_ASSERT_RETURN(logger_, client);
      BS_VERIFY_RETURN(logger_, client->state != State::Closed);

      switch (request->data_case()) {
         case Request::kStartLogin:
            process(client, request->request_id(), request->start_login());
            break;
         case Request::kCancelLogin:
            process(client, request->request_id(), request->cancel_login());
            break;
         case Request::kGetLoginResult:
            process(client, request->request_id(), request->get_login_result());
            break;
         case Request::kLogout:
            process(client, request->request_id(), request->logout());
            break;
         case Request::DATA_NOT_SET:
            break;
      }
   });
}

void BsProxy::onProxyClientConnected(const std::string &clientId)
{
   QMetaObject::invokeMethod(this, [this, clientId] {
      auto it = clients_.find(clientId);
      if (it != clients_.end()) {
         // sanity checks
         SPDLOG_LOGGER_CRITICAL(logger_, "old data was not cleanly removed for client {}!", bs::toHex(clientId));
         clients_.erase(it);
      }

      Client client;
      client.clientId = clientId;
      clients_.emplace(clientId, std::move(client));
   });
}

void BsProxy::onProxyClientDisconnected(const std::string &clientId)
{
   QMetaObject::invokeMethod(this, [this, clientId] {
      // Erase old client's data
      clients_.erase(clientId);
   });
}

void BsProxy::process(Client *client, int64_t requestId, const Request_StartLogin &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process login request from {}", bs::toHex(client->clientId));
   BS_VERIFY_RETURN(logger_, client->state == State::UnknownClient);

   client->autheid = std::make_unique<AutheIDClient>(logger_, nam_, AutheIDClient::AuthKeys{}, params_.autheidTestEnv, this);

   connect(client->autheid.get(), &AutheIDClient::createRequestDone, this, [this, client, requestId] {
      // Data must be still available.
      // if client was disconnected its data should be cleared and autheid was destroyed (and no callback is called).
      BS_ASSERT_RETURN(logger_, client->state == State::WaitAutheidStart);

      Response response;
      auto d = response.mutable_start_login();
      client->state = State::WaitClientGetResult;
      d->set_success(true);
      sendResponse(client, requestId, &response);
   });

   connect(client->autheid.get(), &AutheIDClient::failed, this, [this, client, requestId] {
      // Data must be still available.
      // if client was disconnected its data should be cleared and autheid was destroyed (and no callback is called).
      BS_ASSERT_RETURN(logger_, client->state == State::WaitAutheidStart);

      Response response;
      auto d = response.mutable_start_login();
      client->state = State::Closed;
      d->set_success(false);
      sendResponse(client, requestId, &response);
   });

   client->state = State::WaitAutheidStart;
   client->email = request.email();
   const bool autoRequestResult = false;
   client->autheid->authenticate(request.email(), 120, autoRequestResult);
}

void BsProxy::process(Client *client, int64_t requestId, const Request_CancelLogin &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process cancel login request from {}", bs::toHex(client->clientId));
   BS_VERIFY_RETURN(logger_, client->state == State::WaitAutheidResult || client->state == State::WaitClientGetResult);

   client->state = State::Closed;
   client->autheid->cancel();

   Response response;
   auto d = response.mutable_cancel_login();
   d->set_success(true);
   sendResponse(client, requestId, &response);
}

void BsProxy::process(Client *client, int64_t requestId, const Request_GetLoginResult &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process get login result request from {}", bs::toHex(client->clientId));
   BS_VERIFY_RETURN(logger_, client->state == State::WaitClientGetResult);

   // Need to disconnect `AutheIDClient::failed` signal here because we don't need old callback anymore
   client->autheid->disconnect();

   connect(client->autheid.get(), &AutheIDClient::authSuccess, this, [this, client, requestId](const std::string &jwt) {
      BS_ASSERT_RETURN(logger_, client->state == State::WaitAutheidResult);
      client->state = State::LoggedIn;

      Response response;
      auto d = response.mutable_get_login_result();
      d->set_success(true);
      sendResponse(client, requestId, &response);
   });

   connect(client->autheid.get(), &AutheIDClient::failed, this, [this, client, requestId] {
      BS_ASSERT_RETURN(logger_, client->state == State::WaitAutheidResult);
      client->state = State::Closed;

      Response response;
      auto d = response.mutable_get_login_result();
      d->set_success(false);
      sendResponse(client, requestId, &response);
   });

   connect(client->autheid.get(), &AutheIDClient::userCancelled, this, [this, client, requestId] {
      BS_ASSERT_RETURN(logger_, client->state == State::WaitAutheidResult);
      client->state = State::Closed;

      Response response;
      auto d = response.mutable_get_login_result();
      d->set_success(false);
      sendResponse(client, requestId, &response);
   });

   client->state = State::WaitAutheidResult;
   client->autheid->requestResult();
}

void BsProxy::process(Client *client, int64_t requestId, const Request_Logout &request)
{

}

void BsProxy::sendResponse(Client *client, int64_t requestId, Response *response)
{
   response->set_request_id(requestId);
   server_->SendDataToClient(client->clientId, response->SerializeAsString());
}

BsProxy::Client *BsProxy::findClient(const std::string &clientId)
{
   auto it = clients_.find(clientId);
   if (it == clients_.end()) {
      return nullptr;
   }
   return &it->second;
}
