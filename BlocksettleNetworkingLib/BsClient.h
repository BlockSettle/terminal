#ifndef BS_CLIENT_H
#define BS_CLIENT_H

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <future>
#include <spdlog/logger.h>
#include <QObject>
#include "Address.h"
#include "autheid_utils.h"
#include "AutheIDClient.h"
#include "CelerMessageMapper.h"
#include "DataConnectionListener.h"

class ZmqContext;
class ZmqBIP15XDataConnection;

namespace Blocksettle { namespace Communication { namespace Proxy {
class Request;
class Response_StartLogin;
class Response_GetLoginResult;
class Response_Celer;
class Response;
} } }

struct BsClientParams
{
   struct NewKey
   {
      std::string oldKey;
      std::string newKey;
      std::shared_ptr<std::promise<bool>> prompt;
   };

   using NewKeyCallback = std::function<void(const NewKey &newKey)>;

   std::shared_ptr<ZmqContext> context;

   std::string connectAddress{"127.0.0.1"};
   int connectPort{10259};

   std::string oldServerKey;

   NewKeyCallback newServerKeyCallback;
};

class BsClient : public QObject, public DataConnectionListener
{
   Q_OBJECT
public:
   using SignStartedCb = std::function<void ()>;
   using SignedCb = std::function<void (const AutheIDClient::SignResult &result)>;
   using SignFailedCb = std::function<void(AutheIDClient::ErrorType)>;

   struct SignAddressReq
   {
      enum Type
      {
         Unknown,
         AuthAddr,
         CcAddr,
      };

      Type type{};
      bs::Address address;
      BinaryData invisibleData;
      SignStartedCb startedCb;
      SignedCb signedCb;
      SignFailedCb failedCb;
   };

   BsClient(const std::shared_ptr<spdlog::logger>& logger, const BsClientParams &params
      , QObject *parent = nullptr);
   ~BsClient() override;

   const BsClientParams &params() const { return params_; }

   void startLogin(const std::string &email);

   // Cancel login. Please note that this will close channel.
   void cancelLogin();
   void getLoginResult();
   void logout();
   void celerSend(CelerAPI::CelerMessageType messageType, const std::string &data);

   void signAddress(const SignAddressReq &req);

   static std::chrono::seconds autheidLoginTimeout();
   static std::chrono::seconds autheidAuthAddressTimeout();
   static std::chrono::seconds autheidCcAddressTimeout();
signals:
   void startLoginDone(AutheIDClient::ErrorType status);
   void getLoginResultDone(AutheIDClient::ErrorType status);
   void celerRecv(CelerAPI::CelerMessageType messageType, const std::string &data);

   void connected();
   void disconnected();
   void connectionFailed();
private:
   using ProcessCb = std::function<void(const Blocksettle::Communication::Proxy::Response &response)>;
   using TimeoutCb = std::function<void()>;

   struct ActiveRequest
   {
      ProcessCb processCb;
      TimeoutCb timeoutCb;
   };

   // From DataConnectionListener
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   void sendRequest(Blocksettle::Communication::Proxy::Request *request
      , std::chrono::milliseconds timeout, TimeoutCb timeoutCb, ProcessCb processCb = nullptr);
   void sendMessage(Blocksettle::Communication::Proxy::Request *request);

   void processStartLogin(const Blocksettle::Communication::Proxy::Response_StartLogin &response);
   void processGetLoginResult(const Blocksettle::Communication::Proxy::Response_GetLoginResult &response);
   void processCeler(const Blocksettle::Communication::Proxy::Response_Celer &response);

   void requestSignResult(std::chrono::seconds timeout
      , const BsClient::SignedCb &signedCb, const BsClient::SignFailedCb &failedCb);

   int64_t newRequestId();

   std::shared_ptr<spdlog::logger> logger_;

   BsClientParams params_;

   std::unique_ptr<ZmqBIP15XDataConnection> connection_;

   std::map<int64_t, ActiveRequest> activeRequests_;
   int64_t lastRequestId_{};
};

#endif
