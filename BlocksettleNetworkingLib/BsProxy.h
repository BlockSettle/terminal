#ifndef BS_PROXY_H
#define BS_PROXY_H

#include <functional>
#include <string>
#include <memory>
#include <spdlog/logger.h>
#include <QObject>
#include "Address.h"
#include "DataConnectionListener.h"

class QNetworkAccessManager;
class QThreadPool;
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
class Request_GetLoginResult;
class Request_StartSignAddress;
class Request_GetSignResult;
class Request_CancelLogin;
class Request_Logout;
class Request_Celer;
class Request_CancelSign;
class Response;
} } }

struct BsProxyParams
{
   std::shared_ptr<ZmqContext> context;

   std::string ownKeyFileDir;
   std::string ownKeyFileName;
   std::string ownPubKeyDump;

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
   BsProxy(const std::shared_ptr<spdlog::logger> &logger, const BsProxyParams &params);
   ~BsProxy();

   const BsProxyParams &params() const { return params_; }

   static void overrideCelerHost(const std::string &host, int port);

   // Returns how signed title and description text should look in the mobile device.
   // PB will check it to be sure that the user did sign what he saw.
   // NOTE: If text here will be updated make sure to update both PB and Proxy at the same time.
   static std::string requestTitleAuthAddr();
   static std::string requestDescAuthAddr(const bs::Address &address);
   // NOTE: CC address text details are not enforced on PB right now!
   static std::string requestTitleCcAddr();
   static std::string requestDescCcAddr(const bs::Address &address);
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
      std::unique_ptr<BsClientCelerListener> celerListener;
      std::shared_ptr<DataConnection> celer;
   };

   void onProxyDataFromClient(const std::string& clientId, const std::string& data);
   void onProxyClientConnected(const std::string& clientId);
   void onProxyClientDisconnected(const std::string& clientId);

   void onCelerDataReceived(const std::string& clientId, const std::string& data);
   void onCelerConnected(const std::string& clientId);
   void onCelerDisconnected(const std::string& clientId);
   void onCelerError(const std::string& clientId, DataConnectionListener::DataConnectionError errorCode);

   void processStartLogin(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_StartLogin &request);
   void processGetLoginResult(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_GetLoginResult &request);
   void processStartSignAddress(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_StartSignAddress &request);
   void processGetSignResult(Client *client, int64_t requestId, const Blocksettle::Communication::Proxy::Request_GetSignResult &request);

   void processCancelLogin(Client *client, const Blocksettle::Communication::Proxy::Request_CancelLogin &request);
   void processLogout(Client *client, const Blocksettle::Communication::Proxy::Request_Logout &request);
   void processCeler(Client *client, const Blocksettle::Communication::Proxy::Request_Celer &request);
   void processCancelSign(Client *client, const Blocksettle::Communication::Proxy::Request_CancelSign &request);

   void sendResponse(Client *client, int64_t requestId, Blocksettle::Communication::Proxy::Response *response);
   void sendMessage(Client *client, Blocksettle::Communication::Proxy::Response *response);

   void resetAuthEid(Client *client);

   Client *findClient(const std::string &clientId);

   const std::shared_ptr<spdlog::logger> logger_;

   const BsProxyParams params_;

   std::unordered_map<std::string, Client> clients_;

   std::unique_ptr<BsProxyListener> serverListener_;
   std::unique_ptr<ZmqBIP15XServerConnection> server_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<QNetworkAccessManager> nam_{};
   QThreadPool *threadPool_{};
};

#endif
