#ifndef BS_CLIENT_H
#define BS_CLIENT_H

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <QObject>
#include <spdlog/logger.h>

#include "Address.h"
#include "AutheIDClient.h"
#include "CelerMessageMapper.h"
#include "DataConnectionListener.h"
#include "autheid_utils.h"

class ZmqContext;
class ZmqBIP15XDataConnection;
template<typename T> class FutureValue;

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminal {
         class Request;
         class Response;
         class Response_StartLogin;
         class Response_GetLoginResult;
         class Response_Celer;
         class Response_ProxyPb;
      }
   }
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Request;
         class Response;
      }
   }
}

namespace bs {
   namespace network {
      enum class UserType : int;
   }
}

struct BsClientParams
{
   struct NewKey
   {
      std::string oldKey;
      std::string newKey;
      std::shared_ptr<FutureValue<bool>> prompt;
   };

   using NewKeyCallback = std::function<void(const NewKey &newKey)>;

   std::shared_ptr<ZmqContext> context;

   std::string connectAddress{"127.0.0.1"};
   int connectPort{10259};

   std::string oldServerKey;

   NewKeyCallback newServerKeyCallback;
};

struct BsClientLoginResult
{
   AutheIDClient::ErrorType status{};
   bs::network::UserType userType{};
   std::string celerLogin;
   BinaryData chatTokenData;
   BinaryData chatTokenSign;
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
      std::string srcCcToken;
   };

   BsClient(const std::shared_ptr<spdlog::logger>& logger, const BsClientParams &params
      , QObject *parent = nullptr);
   ~BsClient() override;

   const BsClientParams &params() const { return params_; }

   void startLogin(const std::string &email);

   void sendPbMessage(std::string data);

   // Cancel login. Please note that this will close channel.
   void cancelLogin();
   void getLoginResult();
   void logout();
   void celerSend(CelerAPI::CelerMessageType messageType, const std::string &data);

   void signAddress(const SignAddressReq &req);
   void cancelSign();

   static std::chrono::seconds autheidLoginTimeout();
   static std::chrono::seconds autheidAuthAddressTimeout();
   static std::chrono::seconds autheidCcAddressTimeout();

   // Returns how signed title and description text should look in the mobile device.
   // PB will check it to be sure that the user did sign what he saw.
   // NOTE: If text here will be updated make sure to update both PB and Proxy at the same time.
   static std::string requestTitleAuthAddr();
   static std::string requestDescAuthAddr(const bs::Address &address);
   // NOTE: CC address text details are not enforced on PB right now!
   static std::string requestTitleCcAddr();
   static std::string requestDescCcAddr(const bs::Address &address, const std::string &token);

public slots:
   void sendUnsignedPayin(const std::string& settlementId, const BinaryData& unsignedPayin, const BinaryData& unsignedTxId);
   void sendSignedPayin(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayout(const std::string& settlementId, const BinaryData& signedPayout);

signals:
   void startLoginDone(AutheIDClient::ErrorType status);
   void getLoginResultDone(const BsClientLoginResult &result);

   void celerRecv(CelerAPI::CelerMessageType messageType, const std::string &data);
   // Register Blocksettle::Communication::ProxyTerminalPb::Response with qRegisterMetaType() if queued connection is needed
   void processPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &message);

   void connected();
   void disconnected();
   void connectionFailed();

private:
   using ProcessCb = std::function<void(const Blocksettle::Communication::ProxyTerminal::Response &response)>;
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

   void sendRequest(Blocksettle::Communication::ProxyTerminal::Request *request
      , std::chrono::milliseconds timeout, TimeoutCb timeoutCb, ProcessCb processCb = nullptr);
   void sendMessage(Blocksettle::Communication::ProxyTerminal::Request *request);

   void processStartLogin(const Blocksettle::Communication::ProxyTerminal::Response_StartLogin &response);
   void processGetLoginResult(const Blocksettle::Communication::ProxyTerminal::Response_GetLoginResult &response);
   void processCeler(const Blocksettle::Communication::ProxyTerminal::Response_Celer &response);
   void processProxyPb(const Blocksettle::Communication::ProxyTerminal::Response_ProxyPb &response);

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
