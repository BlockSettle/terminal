#ifndef BS_PROXY_H
#define BS_PROXY_H

#include <functional>
#include <string>
#include <memory>
#include <spdlog/logger.h>
#include <QObject>
#include "DataConnectionListener.h"

class QNetworkAccessManager;

class AutheIDClient;
class ConnectionManager;
class BsProxy;
class BsProxyListener;
class BsClientCelerListener;
class DataConnection;
class ZmqBIP15XServerConnection;
class ZmqContext;

namespace Blocksettle { namespace Communication { namespace Proxy {
class Request_StartLogin;
class Request_CancelLogin;
class Request_GetLoginResult;
class Request_Logout;
class Request_Celer;
class Response;
} } }

struct BsProxyParams
{
   std::shared_ptr<ZmqContext> context;

   std::string ownKeyFileDir;
   std::string ownKeyFileName;

   std::string listenAddress{"127.0.0.1"};
   int listenPort{10259};

   std::string autheidApiKey;
   bool autheidTestEnv{false};

   std::string celerHost;
   int celerPort{};

   // If set BsProxy will check if login is valid before allowing client login
   std::function<bool(BsProxy *proxy, const std::string &login)> verifyCallback;
};

// BsProxy should live in separate QThread to not cause congestion.
// All processing will be done in async on that thread (so there is no need for locks).
// Multiple proxy instances could be started at the same time (need will need to bind to different ports).
class BsProxy : public QObject
{
   Q_OBJECT

public:
   explicit BsProxy(const std::shared_ptr<spdlog::logger> &logger, const BsProxyParams &params);
   ~BsProxy();

   const BsProxyParams &params() const { return params_; }

   static void overrideCelerHost(const std::string &host, int port);
private:
   friend class BsProxyListener;
   friend class BsClientCelerListener;

   enum class State
   {
      UnknownClient,
      WaitAutheidStart,
      WaitClientGetResult,
      WaitAutheidResult,
      LoggedIn,
      Closed,
   };

   struct Client
   {
      std::string clientId;
      std::unique_ptr<AutheIDClient> autheid;
      State state{};
      std::string email;

      // Declare celer listener before celer client itself (it should be destroyed after connection)!
      std::unique_ptr<BsClientCelerListener> celerListener_;
      std::shared_ptr<DataConnection> celer_;
   };

   void onProxyDataFromClient(const std::string& clientId, const std::string& data);
   void onProxyClientConnected(const std::string& clientId);
   void onProxyClientDisconnected(const std::string& clientId);

   void onCelerDataReceived(const std::string& clientId, const std::string& data);
   void onCelerConnected(const std::string& clientId);
   void onCelerDisconnected(const std::string& clientId);
   void onCelerError(const std::string& clientId, DataConnectionListener::DataConnectionError errorCode);

   void processStartLogin(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_StartLogin &request);
   void processCancelLogin(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_CancelLogin &request);
   void processGetLoginResult(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_GetLoginResult &request);
   void processLogout(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_Logout &request);
   void processCeler(Client *client, const Blocksettle::Communication::Proxy::Request_Celer &request);

   void sendResponse(Client *client, int64_t requestId, Blocksettle::Communication::Proxy::Response *response);
   void sendMessage(Client *client, Blocksettle::Communication::Proxy::Response *response);

   Client *findClient(const std::string &clientId);

   const std::shared_ptr<spdlog::logger> logger_;

   const BsProxyParams params_;

   std::unordered_map<std::string, Client> clients_;

   std::unique_ptr<BsProxyListener> serverListener_;
   std::unique_ptr<ZmqBIP15XServerConnection> server_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<QNetworkAccessManager> nam_{};
};

#endif
