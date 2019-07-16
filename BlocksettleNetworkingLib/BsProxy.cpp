#include "BsProxy.h"

#include <QThread>
#include <QNetworkAccessManager>
#include "AutheIDClient.h"
#include "BsClient.h"
#include "CelerMessageMapper.h"
#include "ConnectionManager.h"
#include "DataConnection.h"
#include "DataConnectionListener.h"
#include "StringUtils.h"
#include "ZMQ_BIP15X_ServerConnection.h"
#include "bs_proxy.pb.h"
#include "rp.pb.h"
#include "NettyCommunication.pb.h"
#include "UpstreamLoginProto.pb.h"

using namespace Blocksettle::Communication::Proxy;
using namespace autheid;

namespace {

   ZmqBIP15XPeers emptyTrustedClientsCallback()
   {
      return ZmqBIP15XPeers();
   }

   std::string g_celerHostOverride;
   int g_celerPortOverride;

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

class BsClientCelerListener : public DataConnectionListener
{
public:
   void OnDataReceived(const std::string& data) override
   {
      proxy_->onCelerDataReceived(clientId_, data);
   }

   void OnConnected() override
   {
      proxy_->onCelerConnected(clientId_);
   }

   void OnDisconnected() override
   {
      proxy_->onCelerDisconnected(clientId_);
   }

   void OnError(DataConnectionListener::DataConnectionError errorCode) override
   {
      proxy_->onCelerError(clientId_, errorCode);
   }

   BsProxy *proxy_{};
   std::string clientId_;
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
      SPDLOG_LOGGER_CRITICAL(logger_, "can't bind to {}:{}", params.listenAddress, params.listenPort);
      throw std::runtime_error(fmt::format("can't bind to {}:{}", params.listenAddress, params.listenPort));
   }

   // Need to create own QNetworkAccessManager as a child so it will be in our thread
   nam_ = std::make_shared<QNetworkAccessManager>(this);

   connectionManager_ = std::make_shared<ConnectionManager>(logger_);
}

// static
void BsProxy::overrideCelerHost(const std::string &host, int port)
{
   g_celerHostOverride = host;
   g_celerPortOverride = port;
}

BsProxy::~BsProxy() = default;

void BsProxy::onProxyDataFromClient(const std::string &clientId, const std::string &data)
{
   auto request = std::make_shared<Request>();
   bool result = request->ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse request from {}", bs::toHex(clientId));
      return;
   }

   QMetaObject::invokeMethod(this, [this, clientId, request] {
      auto client = findClient(clientId);
      // Data must be available because we create client's data on connect event
      // (and destroy it on only on disconnect event).
      assert(client);

      if (client->state == State::Closed) {
         SPDLOG_LOGGER_ERROR(logger_, "skip data processing from closed channel for {}", bs::toHex(clientId));
         return;
      }

      switch (request->data_case()) {
         case Request::kStartLogin:
            processStartLogin(client, request->request_id(), request->start_login());
            return;
         case Request::kCancelLogin:
            processCancelLogin(client, request->request_id(), request->cancel_login());
            return;
         case Request::kGetLoginResult:
            processGetLoginResult(client, request->request_id(), request->get_login_result());
            return;
         case Request::kLogout:
            processLogout(client, request->request_id(), request->logout());
            return;
         case Request::kCeler:
            processCeler(client, request->celer());
            return;
         case Request::DATA_NOT_SET:
            return;
      }

      SPDLOG_LOGGER_CRITICAL(logger_, "unknown request detected!");
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

void BsProxy::onCelerDataReceived(const std::string &clientId, const std::string &data)
{
   QMetaObject::invokeMethod(this, [this, clientId, data] {
      auto client = findClient(clientId);
      if (!client) {
         // This is very unlikely but perhaps possible
         SPDLOG_LOGGER_ERROR(logger_, "can't process message from Celer because client is already disconnected");
         return;
      }

      if (client->state != State::LoggedIn) {
         SPDLOG_LOGGER_ERROR(logger_, "got unexpected message from Celer");
         return;
      }

      com::celertech::baseserver::communication::protobuf::ProtobufMessage celerMsg;

      bool result = celerMsg.ParseFromString(data);
      if (!result) {
         SPDLOG_LOGGER_CRITICAL(logger_, "failed to parse ProtobufMessage from Celer");
         return;
      }

      auto messageType = CelerAPI::GetMessageType(celerMsg.protobufclassname());
      if (!CelerAPI::isValidMessageType(messageType)) {
         SPDLOG_LOGGER_CRITICAL(logger_, "get message of unrecognized type: {}", celerMsg.protobufclassname());
         return;
      }

      Response response;
      auto d = response.mutable_celer();
      d->set_message_type(int(messageType));
      d->set_data(std::move(*celerMsg.mutable_protobufmessagecontents()));
      sendMessage(client, &response);
   });
}

void BsProxy::onCelerConnected(const std::string &clientId)
{
   // TODO: Queue requests before we connect to Celer
}

void BsProxy::onCelerDisconnected(const std::string &clientId)
{
   QMetaObject::invokeMethod(this, [this, clientId] {
      // TODO: Close connection and send error to client
   });
}

void BsProxy::onCelerError(const std::string &clientId, DataConnectionListener::DataConnectionError errorCode)
{
   QMetaObject::invokeMethod(this, [this, clientId, errorCode] {
      // TODO: Close connection and send error to client
   });
}

void BsProxy::processStartLogin(Client *client, int64_t requestId, const Request_StartLogin &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process login request from {}", bs::toHex(client->clientId));
   if (client->state != State::UnknownClient) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected start login request");
      return;
   }

   client->autheid = std::make_unique<AutheIDClient>(logger_, nam_, AutheIDClient::AuthKeys{}, params_.autheidTestEnv, this);

   connect(client->autheid.get(), &AutheIDClient::createRequestDone, this, [this, client, requestId] {
      // Check that autheid lives in our thread
      assert(thread() == QThread::currentThread());

      // Data must be still available.
      // if client was disconnected its data should be cleared and autheid was destroyed (and no callback is called).
      if (client->state != State::WaitAutheidStart) {
         SPDLOG_LOGGER_ERROR(logger_, "got unexpected result from Auth eID");
         return;
      }

      Response response;
      auto d = response.mutable_start_login();
      client->state = State::WaitClientGetResult;
      d->set_error_code(int(AutheIDClient::NoError));
      sendResponse(client, requestId, &response);
   });

   connect(client->autheid.get(), &AutheIDClient::failed, this, [this, client, requestId] (AutheIDClient::ErrorType error) {
      // Data must be still available.
      // if client was disconnected its data should be cleared and autheid was destroyed (and no callback is called).
      if (client->state != State::WaitAutheidStart) {
         SPDLOG_LOGGER_ERROR(logger_, "got unexpected result from Auth eID");
         return;
      }

      Response response;
      auto d = response.mutable_start_login();
      client->state = State::Closed;
      d->set_error_code(int(error));
      sendResponse(client, requestId, &response);
   });

   client->state = State::WaitAutheidStart;
   client->email = request.email();
   const bool autoRequestResult = false;
   const int authTimeout = int(BsClient::getDefaultAutheidAuthTimeout() / std::chrono::seconds(1));
   client->autheid->authenticate(request.email(), authTimeout, autoRequestResult);
}

void BsProxy::processCancelLogin(Client *client, int64_t requestId, const Request_CancelLogin &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process cancel login request from {}", bs::toHex(client->clientId));
   if (client->state != State::WaitAutheidResult && client->state != State::WaitClientGetResult) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected cancel login request");
      return;
   }

   // We could safely close connection, terminal will need to start new one if needed
   client->state = State::Closed;
   client->autheid->cancel();
}

void BsProxy::processGetLoginResult(Client *client, int64_t requestId, const Request_GetLoginResult &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process get login result request from {}", bs::toHex(client->clientId));
   if (client->state != State::WaitClientGetResult) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected get login result request");
      return;
   }

   // Need to disconnect `AutheIDClient::failed` signal here because we don't need old callbacks anymore
   client->autheid->disconnect();

   connect(client->autheid.get(), &AutheIDClient::authSuccess, this, [this, client, requestId](const std::string &jwt) {
      if (client->state != State::WaitAutheidResult) {
         SPDLOG_LOGGER_ERROR(logger_, "got unexpected result from Auth eID");
         return;
      }

      client->state = State::LoggedIn;

      Response response;
      auto d = response.mutable_get_login_result();
      d->set_error_code(int(AutheIDClient::NoError));
      sendResponse(client, requestId, &response);

      client->celerListener_ = std::make_unique<BsClientCelerListener>();
      client->celerListener_->proxy_ = this;
      client->celerListener_->clientId_ = client->clientId;

      client->celer_ = connectionManager_->CreateCelerClientConnection();

      const std::string &host = g_celerHostOverride.empty() ? params_.celerHost : g_celerHostOverride;
      const int port = g_celerHostOverride.empty() ? params_.celerPort : g_celerPortOverride;

      client->celer_->openConnection(host, std::to_string(port), client->celerListener_.get());
   });

   connect(client->autheid.get(), &AutheIDClient::failed, this, [this, client, requestId](AutheIDClient::ErrorType error) {
      if (client->state != State::WaitAutheidResult) {
         SPDLOG_LOGGER_ERROR(logger_, "got unexpected result from Auth eID");
         return;
      }

      client->state = State::Closed;

      Response response;
      auto d = response.mutable_get_login_result();
      d->set_error_code(int(error));
      sendResponse(client, requestId, &response);
   });

   connect(client->autheid.get(), &AutheIDClient::userCancelled, this, [this, client, requestId] {
      if (client->state != State::WaitAutheidResult) {
         SPDLOG_LOGGER_ERROR(logger_, "got unexpected result from Auth eID");
         return;
      }

      // Request was cancelled from mobile device.
      // Allow client try login one more time.
      client->state = State::UnknownClient;

      Response response;
      auto d = response.mutable_get_login_result();
      d->set_error_code(int(AutheIDClient::Cancelled));
      sendResponse(client, requestId, &response);
   });

   client->state = State::WaitAutheidResult;
   client->autheid->requestResult();
}

void BsProxy::processLogout(Client *client, int64_t requestId, const Request_Logout &request)
{
   // TODO: Close Celer connection
}

void BsProxy::processCeler(BsProxy::Client *client, const Request_Celer &request)
{
   if (client->state != State::LoggedIn) {
      SPDLOG_LOGGER_ERROR(logger_, "got unexpected Celer message from client");
      return;
   }

   auto messageType = CelerAPI::CelerMessageType(request.message_type());
   if (!CelerAPI::isValidMessageType(messageType)) {
      SPDLOG_LOGGER_ERROR(logger_, "get message of invalid type ({}) from client {}/{}"
         , request.message_type(), client->clientId, client->email);
      return;
   }

   std::string dataOverride = request.data();

   if (messageType == CelerAPI::LoginRequestType) {
      // Override user's login and password here
      com::celertech::baseserver::communication::login::LoginRequest loginRequest;
      loginRequest.set_username(client->email);
      // FIXME: Use different passwords
      loginRequest.set_password("Welcome1234");
      dataOverride = loginRequest.SerializeAsString();
   }

   std::string fullClassName = CelerAPI::GetMessageClass(messageType);
   // CelerAPI::isValidMessageType check at the start must prevent this
   assert(!fullClassName.empty());

   com::celertech::baseserver::communication::protobuf::ProtobufMessage message;
   message.set_protobufclassname(fullClassName);
   message.set_protobufmessagecontents(dataOverride);
   client->celer_->send(message.SerializeAsString());
}

void BsProxy::sendResponse(Client *client, int64_t requestId, Response *response)
{
   response->set_request_id(requestId);
   sendMessage(client, response);
}

void BsProxy::sendMessage(BsProxy::Client *client, Response *response)
{
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
