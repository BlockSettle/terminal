#ifndef __CELER_CLIENT_H__
#define __CELER_CLIENT_H__

#include <memory>
#include <string>
#include <queue>
#include <functional>
#include <unordered_set>
#include <unordered_map>

#include <QObject>
#include <QTimer>

#include "DataConnectionListener.h"
#include "CelerMessageMapper.h"
#include "CelerProperty.h"
#include "IdStringGenerator.h"

class ConnectionManager;
class CommandSequence;
class DataConnection;
class BaseCelerCommand;
class CelerClientListener;

namespace spdlog {
   class logger;
};

class CelerClient : public QObject
{
Q_OBJECT

friend class CelerClientListener;

public:
   enum CelerErrorCode
   {
      ResolveHostError,
      LoginError,
      ServerMaintainanceError,
      UndefinedError
   };

   using message_handler = std::function<bool (const std::string&)>;

public:
   CelerClient(const std::shared_ptr<ConnectionManager>& connectionManager, bool userIdRequired = true);
   ~CelerClient() noexcept override = default;

   CelerClient(const CelerClient&) = delete;
   CelerClient& operator = (const CelerClient&) = delete;

   CelerClient(CelerClient&&) = delete;
   CelerClient& operator = (CelerClient&&) = delete;

   bool LoginToServer(const std::string& hostname, const std::string& port
      , const std::string& login, const std::string& password);

   bool RegisterHandler(CelerAPI::CelerMessageType messageType, const message_handler& handler);

   bool ExecuteSequence(const std::shared_ptr<BaseCelerCommand>& command);

   bool IsConnected() const;

   std::string userName() const;
   std::string userId() const;

   bool tradingAllowed() const;

   std::string getUserOtpId() const;

   uint64_t    getUserOtpUsedCount() const;
   bool        setUserOtpUsedCount(uint64_t count);

   std::unordered_set<std::string> GetSubmittedAuthAddressSet() const;
   bool SetSubmittedAuthAddressSet(const std::unordered_set<std::string>& addressSet);

   bool IsCCAddressSubmitted (const std::string &address) const;
   bool SetCCAddressSubmitted(const std::string &address);

public slots:
   void CloseConnection();
   void sendHeartbeat();

   void onTimerRestart();
signals:
   void OnConnectedToServer();
   void OnConnectionClosed();
   void OnConnectionError(int errorCode);

   void closingConnection();
   void restartTimer();
private:
   void OnDataReceived(const std::string& data);
   void OnConnected();
   void OnDisconnected();
   void OnError(DataConnectionListener::DataConnectionError errorCode);

   void RegisterDefaulthandlers();

   bool sendMessage(CelerAPI::CelerMessageType messageType, const std::string& data);

   void AddInternalSequence(const std::shared_ptr<BaseCelerCommand>& commandSequence);

   void SendCommandMessagesIfRequired(const std::shared_ptr<BaseCelerCommand>& command);
   void RegisterUserCommand(const std::shared_ptr<BaseCelerCommand>& command);
private:
   bool onHeartbeat(const std::string& message);
   bool onSignleMessage(const std::string& message);
   bool onMultiMessage(const std::string& message);

   bool SendDataToSequence(const std::string& sequenceId, CelerAPI::CelerMessageType messageType, const std::string& message);

   void loginSuccessCallback(const std::string& userName, const std::string& sessionToken, int32_t heartbeatInterval);
   void loginFailedCallback(const std::string& errorMessage);

   void UpdateSetFromString(const std::string& value, std::unordered_set<std::string> &set);
   void AddToSet(const std::string& address, std::unordered_set<std::string> &set);

   std::string SetToString(const std::unordered_set<std::string> &set);
private:
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<CelerClientListener>   listener_;
   std::shared_ptr<DataConnection>        connection_;
   QTimer                                 *heartbeatTimer_;

   using commandsQueueType = std::queue< std::shared_ptr<BaseCelerCommand> >;
   commandsQueueType internalCommands_;

   std::unordered_map<CelerAPI::CelerMessageType, message_handler, std::hash<int> > messageHandlersMap_;
   std::unordered_map<std::string, std::shared_ptr<BaseCelerCommand>>               activeCommands_;

   std::string sessionToken_;
   std::string userName_;
   CelerProperty userId_;
   CelerProperty bitcoinParticipant_;

   CelerProperty        submittedAuthAddressListProperty_;
   std::unordered_set<std::string> submittedAuthAddressSet_;

   CelerProperty        submittedCCAddressListProperty_;
   std::unordered_set<std::string> submittedCCAddressSet_;

   CelerProperty otpId_;
   CelerProperty otpIndex_;

   int32_t     heartbeatInterval_;

   IdStringGenerator       idGenerator_;
   bool                    userIdRequired_;

   bool serverNotAvailable_;
};

#endif // __CELER_CLIENT_H__
