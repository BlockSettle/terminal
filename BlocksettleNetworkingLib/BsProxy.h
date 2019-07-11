#ifndef BS_PROXY_H
#define BS_PROXY_H

#include <functional>
#include <string>
#include <memory>
#include <spdlog/logger.h>
#include <QObject>

class AutheIDClient;
class QNetworkAccessManager;
class BsProxy;
class BsProxyListener;
class ZmqBIP15XServerConnection;
class ZmqContext;

namespace Blocksettle { namespace Communication { namespace Proxy {
class Request_StartLogin;
class Request_CancelLogin;
class Request_GetLoginResult;
class Request_Logout;
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

class BsProxy : public QObject
{
   Q_OBJECT

public:
   explicit BsProxy(const std::shared_ptr<spdlog::logger> &logger, const BsProxyParams &params);
   ~BsProxy();

   const BsProxyParams &params() const { return params_; }
private:
   friend class BsProxyListener;

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
   };

   void onProxyDataFromClient(const std::string& clientId, const std::string& data);
   void onProxyClientConnected(const std::string& clientId);
   void onProxyClientDisconnected(const std::string& clientId);

   void process(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_StartLogin &request);
   void process(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_CancelLogin &request);
   void process(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_GetLoginResult &request);
   void process(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_Logout &request);

   void sendResponse(Client *client, int64_t requestId, Blocksettle::Communication::Proxy::Response *response);

   Client *findClient(const std::string &clientId);

   const std::shared_ptr<spdlog::logger> logger_;

   const BsProxyParams params_;

   std::unordered_map<std::string, Client> clients_;

   std::unique_ptr<BsProxyListener> serverListener_;
   std::unique_ptr<ZmqBIP15XServerConnection> server_;
   std::shared_ptr<QNetworkAccessManager> nam_{};
};

#endif
