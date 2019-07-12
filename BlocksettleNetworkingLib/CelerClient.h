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

   enum CelerUserType
   {
      Undefined,
      Dealing,
      Trading,
      Market
   };

   using message_handler = std::function<bool (const std::string&)>;

public:
   CelerClient(const std::shared_ptr<spdlog::logger> &logger, bool userIdRequired = true);
   ~CelerClient() noexcept override = default;

   CelerClient(const CelerClient&) = delete;
   CelerClient& operator = (const CelerClient&) = delete;

   CelerClient(CelerClient&&) = delete;
   CelerClient& operator = (CelerClient&&) = delete;

   bool LoginToServer(const std::string& login, const std::string& password);

   bool RegisterHandler(CelerAPI::CelerMessageType messageType, const message_handler& handler);

   bool ExecuteSequence(const std::shared_ptr<BaseCelerCommand>& command);

   bool IsConnected() const;

   std::string userName() const;
   std::string userId() const;
   const QString& userType() const;
   CelerUserType celerUserType() const;

   bool tradingAllowed() const;

   std::unordered_set<std::string> GetSubmittedAuthAddressSet() const;
   bool SetSubmittedAuthAddressSet(const std::unordered_set<std::string>& addressSet);

   bool IsCCAddressSubmitted (const std::string &address) const;
   bool SetCCAddressSubmitted(const std::string &address);

   static void UpdateSetFromString(const std::string& value, std::unordered_set<std::string> &set);
   static std::string SetToString(const std::unordered_set<std::string> &set);

   virtual void CloseConnection();

public slots:
   void sendHeartbeat();

   void onTimerRestart();
   void recvData(CelerAPI::CelerMessageType messageType, const std::string &data);

signals:
   void OnConnectedToServer();
   void OnConnectionClosed();
   void OnConnectionError(int errorCode);

   void closingConnection();
   void restartTimer();

   void sendData(CelerAPI::CelerMessageType messageType, const std::string &data);

private:
   void OnDataReceived(CelerAPI::CelerMessageType messageType, const std::string& data);
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
   bool onSingleMessage(const std::string& message);
   bool onExceptionResponse(const std::string& message);
   bool onMultiMessage(const std::string& message);

   bool SendDataToSequence(const std::string& sequenceId, CelerAPI::CelerMessageType messageType, const std::string& message);

   void loginSuccessCallback(const std::string& userName, const std::string& sessionToken, int32_t heartbeatInterval);
   void loginFailedCallback(const std::string& errorMessage);

   static void AddToSet(const std::string& address, std::unordered_set<std::string> &set);

protected:
   std::shared_ptr<spdlog::logger> logger_;

private:
   QTimer                                 *heartbeatTimer_;

   using commandsQueueType = std::queue< std::shared_ptr<BaseCelerCommand> >;
   commandsQueueType internalCommands_;

   std::unordered_map<CelerAPI::CelerMessageType, message_handler, std::hash<int> > messageHandlersMap_;
   std::unordered_map<std::string, std::shared_ptr<BaseCelerCommand>>               activeCommands_;

   std::string sessionToken_;
   std::string userName_;
   QString userType_;
   CelerUserType celerUserType_;
   CelerProperty userId_;
   CelerProperty bitcoinParticipant_;

   CelerProperty        submittedAuthAddressListProperty_;
   std::unordered_set<std::string> submittedAuthAddressSet_;

   CelerProperty        submittedCCAddressListProperty_;
   std::unordered_set<std::string> submittedCCAddressSet_;

   int32_t     heartbeatInterval_;

   IdStringGenerator       idGenerator_;
   bool                    userIdRequired_;

   bool serverNotAvailable_;
};

#endif // __CELER_CLIENT_H__
