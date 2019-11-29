/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef BASE_CELER_CLIENT_H
#define BASE_CELER_CLIENT_H

#include <memory>
#include <string>
#include <queue>
#include <functional>
#include <unordered_set>
#include <unordered_map>

#include <QObject>
#include <QTimer>

#include "CelerMessageMapper.h"
#include "CelerProperty.h"
#include "CommonTypes.h"
#include "DataConnectionListener.h"
#include "IdStringGenerator.h"

class CommandSequence;
class DataConnection;
class BaseCelerCommand;
class CelerClientListener;

namespace spdlog {
   class logger;
};

class BaseCelerClient : public QObject
{
Q_OBJECT

friend class CelerClientListener;

public:
   using CelerUserType = bs::network::UserType;

   enum CelerErrorCode
   {
      ResolveHostError,
      LoginError,
      ServerMaintainanceError,
      UndefinedError
   };

   using message_handler = std::function<bool (const std::string&)>;

public:
   BaseCelerClient(const std::shared_ptr<spdlog::logger> &logger, bool userIdRequired, bool useRecvTimer);
   ~BaseCelerClient() noexcept override = default;

   BaseCelerClient(const BaseCelerClient&) = delete;
   BaseCelerClient& operator = (const BaseCelerClient&) = delete;

   BaseCelerClient(BaseCelerClient&&) = delete;
   BaseCelerClient& operator = (BaseCelerClient&&) = delete;

   bool RegisterHandler(CelerAPI::CelerMessageType messageType, const message_handler& handler);

   bool ExecuteSequence(const std::shared_ptr<BaseCelerCommand>& command);

   bool IsConnected() const;

   // For CelerClient userName and email are the same.
   // For CelerClientProxy they are different!
   // Requests to Celer should always use userName, requests to PB and Genoa should use email.
   const std::string& userName() const { return userName_; }

   // Email will be always in lower case here
   const std::string& email() const { return email_; }

   const std::string& userId() const;
   const QString& userType() const { return userType_; }
   CelerUserType celerUserType() const { return celerUserType_; }

   bool tradingAllowed() const;

   std::unordered_set<std::string> GetSubmittedAuthAddressSet() const;
   bool SetSubmittedAuthAddressSet(const std::unordered_set<std::string>& addressSet);

   bool IsCCAddressSubmitted (const std::string &address) const;
   bool SetCCAddressSubmitted(const std::string &address);

   static void UpdateSetFromString(const std::string& value, std::unordered_set<std::string> &set);
   static std::string SetToString(const std::unordered_set<std::string> &set);

   virtual void CloseConnection();

signals:
   void OnConnectedToServer();
   void OnConnectionClosed();
   void OnConnectionError(int errorCode);

   void closingConnection();

protected:
   // Override to do actual data send
   virtual void onSendData(CelerAPI::CelerMessageType messageType, const std::string &data) = 0;

   // Call when there is new data was received
   void recvData(CelerAPI::CelerMessageType messageType, const std::string &data);

   // Call when there is need to send login request
   bool SendLogin(const std::string& login, const std::string& email, const std::string& password);

   std::shared_ptr<spdlog::logger> logger_;

private slots:
   void onSendHbTimeout();
   void onRecvHbTimeout();

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

   bool onHeartbeat(const std::string& message);
   bool onSingleMessage(const std::string& message);
   bool onExceptionResponse(const std::string& message);
   bool onMultiMessage(const std::string& message);

   bool SendDataToSequence(const std::string& sequenceId, CelerAPI::CelerMessageType messageType, const std::string& message);

   void loginSuccessCallback(const std::string& userName, const std::string& email, const std::string& sessionToken, std::chrono::seconds heartbeatInterval);
   void loginFailedCallback(const std::string& errorMessage);

   static void AddToSet(const std::string& address, std::unordered_set<std::string> &set);

   QTimer                                 *timerSendHb_{};
   QTimer                                 *timerRecvHb_{};

   using commandsQueueType = std::queue< std::shared_ptr<BaseCelerCommand> >;
   commandsQueueType internalCommands_;

   std::unordered_map<CelerAPI::CelerMessageType, message_handler, std::hash<int> > messageHandlersMap_;
   std::unordered_map<std::string, std::shared_ptr<BaseCelerCommand>>               activeCommands_;

   std::string sessionToken_;
   std::string userName_;
   std::string email_;
   QString userType_;
   CelerUserType celerUserType_;
   CelerProperty userId_;
   CelerProperty bitcoinParticipant_;

   CelerProperty        submittedAuthAddressListProperty_;
   std::unordered_set<std::string> submittedAuthAddressSet_;

   CelerProperty        submittedCCAddressListProperty_;
   std::unordered_set<std::string> submittedCCAddressSet_;

   std::chrono::seconds    heartbeatInterval_{};

   IdStringGenerator       idGenerator_;
   bool                    userIdRequired_;

   bool serverNotAvailable_;
};

#endif
