#include "BsProxy.h"

#include <QNetworkAccessManager>
#include "AutheIDClient.h"
#include "StringUtils.h"
#include "ZMQ_BIP15X_ServerConnection.h"

#include "bs_proxy.pb.h"
#include "rp.pb.h"

using namespace Blocksettle::Communication::Proxy;
using namespace autheid;

namespace {

   const auto ContentTypeHeader = "Content-Type";
   const auto AcceptHeader = "Accept";
   const auto AuthorizationHeader = "Authorization";

   const auto ProtobufMimeType = "application/protobuf";

   const auto AutheidServerAddrLive = "https://api.autheid.com/v1/requests";
   const auto AutheidServerAddrTest = "https://api.staging.autheid.com/v1/requests";

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

   nam_ = new QNetworkAccessManager(this);
}

BsProxy::~BsProxy() = default;

void BsProxy::onProxyDataFromClient(const std::string &clientId, const std::string &data)
{
   Request requst;
   bool result = requst.ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse request from {}", bs::toHex(clientId));
   }

   switch (requst.data_case()) {
      case Request::kStartLogin:
         process(clientId, requst.start_login());
         break;
      case Request::kCancelLogin:
         process(clientId, requst.cancel_login());
         break;
      case Request::kGetLoginResult:
         process(clientId, requst.get_login_result());
         break;
      case Request::kLogout:
         process(clientId, requst.logout());
         break;
      case Request::DATA_NOT_SET:
         break;
   }
}

void BsProxy::onProxyClientConnected(const std::string &clientId)
{

}

void BsProxy::onProxyClientDisconnected(const std::string &clientId)
{

}

void BsProxy::process(const std::string &clientId, const Request_StartLogin &request)
{
   SPDLOG_LOGGER_INFO(logger_, "process login request from {}", bs::toHex(clientId));

   rp::CreateRequest autheidReq;
   auto signRequest = autheidReq.mutable_signature();
   signRequest->set_serialization(rp::SERIALIZATION_PROTOBUF);

   autheidReq.set_title("Terminal Login");
   autheidReq.set_type(rp::SIGNATURE);
   autheidReq.set_email(request.auth_id());
   autheidReq.set_timeout_seconds(60);

   QNetworkRequest httpReq;
   httpReq.setUrl(QUrl(QString::fromLatin1(params_.autheidTestEnv ? AutheidServerAddrTest : AutheidServerAddrLive)));
   httpReq.setRawHeader(ContentTypeHeader, ProtobufMimeType);
   httpReq.setRawHeader(AcceptHeader, ProtobufMimeType);
   httpReq.setRawHeader(AuthorizationHeader, QByteArray::fromStdString(params_.autheidApiKey));
   //nam_->post()
}

void BsProxy::process(const std::string &clientId, const Request_CancelLogin &request)
{

}

void BsProxy::process(const std::string &clientId, const Request_GetLoginResult &request)
{

}

void BsProxy::process(const std::string &clientId, const Request_Logout &request)
{

}
