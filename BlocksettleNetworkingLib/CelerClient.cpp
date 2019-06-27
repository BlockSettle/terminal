#include "CelerClient.h"

#include "DataConnection.h"
#include "ConnectionManager.h"

#include "CelerLoginSequence.h"
#include "CelerGetUserIdSequence.h"
#include "CelerGetUserPropertySequence.h"
#include "CelerLoadUserInfoSequence.h"
#include "CelerPropertiesDefinitions.h"
#include "CelerSetUserPropertySequence.h"

#include "NettyCommunication.pb.h"

using namespace com::celertech::baseserver::communication::protobuf;

class CelerClientListener : public DataConnectionListener
{
public:
   CelerClientListener(CelerClient *client)
      : client_(client)
   {
   }

   ~CelerClientListener() noexcept
   {}

public:
   void OnDataReceived(const std::string& data) override
   {
      client_->OnDataReceived(data);
   }
   void OnConnected() override
   {
      client_->OnConnected();
   }
   void OnDisconnected() override
   {
      client_->OnDisconnected();
   }
   void OnError(DataConnectionListener::DataConnectionError errorCode) override
   {
      client_->OnError(errorCode);
   }

private:
   CelerClient *client_;
};

CelerClient::CelerClient(const std::shared_ptr<ConnectionManager>& connectionManager, bool userIdRequired)
   : connectionManager_(connectionManager)
   , logger_(connectionManager->GetLogger())
   , userId_(CelerUserProperties::UserIdPropertyName)
   , submittedAuthAddressListProperty_(CelerUserProperties::SubmittedBtcAuthAddressListPropertyName)
   , submittedCCAddressListProperty_(CelerUserProperties::SubmittedCCAddressListPropertyName)
   , userIdRequired_(userIdRequired)
   , serverNotAvailable_(false)
{
   heartbeatTimer_ = new QTimer(this);
   heartbeatTimer_->setInterval(30 * 1000);
   heartbeatTimer_->setSingleShot(false);

   connect(heartbeatTimer_, &QTimer::timeout, this, &CelerClient::sendHeartbeat);
   connect(this, &CelerClient::restartTimer, this, &CelerClient::onTimerRestart);

   connect(this, &CelerClient::closingConnection, this, &CelerClient::CloseConnection, Qt::QueuedConnection);
   RegisterDefaulthandlers();

   celerUserType_ = CelerUserType::Undefined;
}

bool CelerClient::LoginToServer(const std::string& hostname, const std::string& port
      , const std::string& login, const std::string& password)
{
   if (connection_) {
      logger_->error("[CelerClient::LoginToServer] connecting with not purged connection");
      return false;
   }

   // create user login sequence
   std::string loginString = login;
   sessionToken_.clear();

   celerUserType_ = CelerUserType::Undefined;

   auto loginSequence = std::make_shared<CelerLoginSequence>(logger_, login, password);
   auto onLoginSuccess = [this,loginString](const std::string& sessionToken, int32_t heartbeatInterval) {
     loginSuccessCallback(loginString, sessionToken, heartbeatInterval);
   };
   auto onLoginFailed = [this](const std::string& errorMessage) {
     loginFailedCallback(errorMessage);
   };
   loginSequence->SetCallbackFunctions(onLoginSuccess, onLoginFailed);

   AddInternalSequence(loginSequence);

   listener_ = std::make_shared<CelerClientListener>(this);
   connection_ = connectionManager_->CreateCelerClientConnection();

   // put login command to queue
   if (!connection_->openConnection(hostname, port, listener_.get())) {
      logger_->error("[CelerClient::LoginToServer] failed to open celer connection");
      // XXX purge connection and listener
      connection_.reset();
      return false;
   }

   return true;
}

void CelerClient::loginSuccessCallback(const std::string& userName, const std::string& sessionToken
   , int32_t heartbeatInterval)
{
   logger_->debug("[CelerClient::loginSuccessCallback] logged in as {}", userName);
   userName_ = userName;
   sessionToken_ = sessionToken;
   heartbeatInterval_ = heartbeatInterval;
   serverNotAvailable_ = false;
   idGenerator_.setUserName(userName);

   heartbeatTimer_->setInterval(heartbeatInterval_ * 1000);
   heartbeatTimer_->setSingleShot(false);

   if (userIdRequired_) {
      auto getUserIdSequence = std::make_shared<CelerLoadUserInfoSequence>(logger_, userName, [this](CelerProperties properties) {
         userId_ = properties[CelerUserProperties::UserIdPropertyName];
         bitcoinParticipant_ = properties[CelerUserProperties::BitcoinParticipantPropertyName];

         const auto authIt = properties.find(CelerUserProperties::SubmittedBtcAuthAddressListPropertyName);
         if (authIt != properties.end()) {
            submittedAuthAddressListProperty_ = authIt->second;
            UpdateSetFromString(submittedAuthAddressListProperty_.value, submittedAuthAddressSet_);
         }

         const auto ccIt = properties.find(CelerUserProperties::SubmittedCCAddressListPropertyName);
         if (ccIt != properties.end()) {
            submittedCCAddressListProperty_ = properties[CelerUserProperties::SubmittedCCAddressListPropertyName];
            UpdateSetFromString(submittedCCAddressListProperty_.value, submittedCCAddressSet_);
         }

         const bool bd = (properties[CelerUserProperties::BitcoinDealerPropertyName].value == "true");
         const bool bp = (bitcoinParticipant_.value == "true");

         if (bp && bd) {
            userType_ = tr("Dealing Participant");
            celerUserType_ = CelerUserType::Dealing;
         } else if (bp && !bd) {
            userType_ = tr("Trading Participant");
            celerUserType_ = CelerUserType::Trading;
         } else {
            userType_ = tr("Market Participant");
            celerUserType_ = CelerUserType::Market;
         }

         emit OnConnectedToServer();
      });
      ExecuteSequence(getUserIdSequence);
   } else {
      emit OnConnectedToServer();
   }

   emit restartTimer();
}

void CelerClient::loginFailedCallback(const std::string& errorMessage)
{
   if (errorMessage == "Server not available, please try again later.") {
      if (!serverNotAvailable_){
         logger_->error("[CelerClient] login failed: {}", errorMessage);
         serverNotAvailable_ = true;
      }

      emit OnConnectionError(ServerMaintainanceError);
   } else {
      logger_->error("[CelerClient] login failed: {}", errorMessage);
      emit OnConnectionError(LoginError);
   }
}

void CelerClient::AddInternalSequence(const std::shared_ptr<BaseCelerCommand>& commandSequence)
{
   internalCommands_.emplace(commandSequence);
}

void CelerClient::CloseConnection()
{
   heartbeatTimer_->stop();

   if (connection_) {
      connection_->closeConnection();
      connection_.reset();
      emit OnConnectionClosed();
   }
}

void CelerClient::OnDataReceived(const std::string& data)
{
   ProtobufMessage response;

   // Without adding \0 it will not parse
   if (response.ParseFromString(data+'\0')) {
      logger_->error("[CelerClient::OnDataReceived] failed to parse ProtobufMessage");
      return;
   }

   auto messageType = CelerAPI::GetMessageType(response.protobufclassname());
   if (messageType == CelerAPI::UndefinedType) {
      logger_->error("[CelerClient::OnDataReceived] get message of unrecognized type : {}", response.protobufclassname());
      return;
   }

   // internal queue
   while (!internalCommands_.empty()) {
      std::shared_ptr<BaseCelerCommand> command = internalCommands_.front();
      if (command->IsCompleted()) {
         logger_->error("[CelerClient::OnDataReceived] ========== Completed command in internal Q =========");
         internalCommands_.pop();
         command->FinishSequence();
         continue;
      }

      if (command->IsWaitingForData()) {
         if (command->OnMessage({messageType, response.protobufmessagecontents()}) ) {
            SendCommandMessagesIfRequired(command);
         }

         if (command->IsCompleted()) {
            internalCommands_.pop();
            command->FinishSequence();
         }
         return;
      } else {
         logger_->debug("[CelerClient::OnDataReceived] internal command {} not waiting for message {}"
            , command->GetCommandName(), response.protobufclassname());
         break;
      }
   }

   auto handlerIt = messageHandlersMap_.find(messageType);
   if (handlerIt != messageHandlersMap_.end()) {
      if (handlerIt->second(response.protobufmessagecontents())) {
         return;
      }
      logger_->debug("[CelerClient::OnDataReceived] handler rejected message of type {}.", response.protobufclassname());
   } else {
      logger_->debug("[CelerClient::OnDataReceived] ignore message {}", response.protobufclassname());
   }
}

void CelerClient::SendCommandMessagesIfRequired(const std::shared_ptr<BaseCelerCommand>& command)
{
   while (!(command->IsCompleted() || command->IsWaitingForData())) {
      auto message = command->GetNextDataToSend();
      sendMessage(message.messageType, message.messageData);
   }
}

bool CelerClient::sendMessage(CelerAPI::CelerMessageType messageType, const std::string& data)
{
   ProtobufMessage message;

   // reset heartbeat interval
   if (heartbeatTimer_->isActive()) {
      emit restartTimer();
   }

   std::string fullClassName = CelerAPI::GetMessageClass(messageType);
   if (fullClassName.empty()) {
      logger_->error("[CelerClient::sendMessage] could not get class name for {}", messageType);
      return false;
   }

   message.set_protobufclassname(fullClassName);
   message.set_protobufmessagecontents(data);

   return connection_->send(message.SerializeAsString());
}

void CelerClient::OnConnected()
{
   // start command execution
   if (!internalCommands_.empty()) {
      if (!internalCommands_.front()->IsWaitingForData()) {
         auto message = internalCommands_.front()->GetNextDataToSend();
         sendMessage(message.messageType, message.messageData);
      }
   } else {
      logger_->error("[CelerClient::OnConnected] ======= no internal sequence =======");
   }
}

void CelerClient::OnDisconnected()
{
//   commandsQueueType{}.swap(internalCommands_);
   emit closingConnection();
}

void CelerClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   // emit error message
   CelerErrorCode celerError = UndefinedError;

   switch (errorCode) {
   case DataConnectionListener::HostNotFoundError:
      celerError = ResolveHostError;
      break;
   default:
      break;
   }

   emit OnConnectionError(celerError);
}

void CelerClient::RegisterDefaulthandlers()
{
   RegisterHandler(CelerAPI::HeartbeatType, [this](const std::string& data) { return this->onHeartbeat(data); });
   RegisterHandler(CelerAPI::SingleResponseMessageType, [this](const std::string& data) { return this->onSignleMessage(data); });
   RegisterHandler(CelerAPI::ExceptionResponseMessageType, [this](const std::string& data) { return this->onExceptionResponse(data); });
   RegisterHandler(CelerAPI::MultiResponseMessageType, [this](const std::string& data) { return this->onMultiMessage(data); });
}

bool CelerClient::RegisterHandler(CelerAPI::CelerMessageType messageType, const message_handler& handler)
{
   auto it = messageHandlersMap_.find(messageType);
   if (it != messageHandlersMap_.end()) {
      logger_->error("[CelerClient::RegisterHandler] handler for message {} already exists", messageType);
      return false;
   }

   messageHandlersMap_.emplace(messageType, handler);

   return true;
}

bool CelerClient::onHeartbeat(const std::string& message)
{
   Heartbeat response;

   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onHeartbeat] failed to parse message");
      return false;
   }

   return true;
}

bool CelerClient::onSignleMessage(const std::string& message)
{
   SingleResponseMessage response;
   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onSignleMessage] failed to parse SingleResponseMessage");
      return false;
   }

   return SendDataToSequence(response.clientrequestid(), CelerAPI::SingleResponseMessageType, message);
}

bool CelerClient::onExceptionResponse(const std::string& message)
{
   ExceptionResponseMessage response;

   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onExceptionResponse] failed to parse ExceptionResponseMessage");
      return false;
   }

   logger_->error("[CelerClient::onExceptionResponse] get exception response: {}"
      , response.DebugString());

   return true;
}

bool CelerClient::onMultiMessage(const std::string& message)
{
   MultiResponseMessage response;
   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onMultiMessage] failed to parse MultiResponseMessage");
      return false;
   }

   return SendDataToSequence(response.clientrequestid(), CelerAPI::MultiResponseMessageType, message);
}

bool CelerClient::SendDataToSequence(const std::string& sequenceId, CelerAPI::CelerMessageType messageType, const std::string& message)
{
   auto commandIt = activeCommands_.find(sequenceId);
   if (commandIt == activeCommands_.end()) {
      logger_->error("[CelerClient::SendDataToSequence] there is no active sequence for id {}", sequenceId);
      return false;
   }

   std::shared_ptr<BaseCelerCommand> command = commandIt->second;
   if (command->IsWaitingForData()) {
      if (command->OnMessage( {messageType, message} ) ) {
         SendCommandMessagesIfRequired(command);
      }

      if (command->IsCompleted()) {
         command->FinishSequence();
         activeCommands_.erase(commandIt);
      }

   } else {
      logger_->error("[CelerClient::SendDataToSequence] command is not waiting for data {}", sequenceId);
      return false;
   }

   return true;
}

void CelerClient::sendHeartbeat()
{
   Heartbeat heartbeat;

   sendMessage(CelerAPI::HeartbeatType, heartbeat.SerializeAsString());
}

bool CelerClient::ExecuteSequence(const std::shared_ptr<BaseCelerCommand>& command)
{
   if (!IsConnected()) {
      logger_->error("[CelerClient::ExecuteSequence] could not execute seqeuence if not connected");
      return false;
   }

   if (command->IsCompleted()) {
      logger_->error("[CelerClient::ExecuteSequence] could not execute completed sequence");
      return false;
   }

   if (command->IsWaitingForData()) {
      logger_->error("[CelerClient::ExecuteSequence] could not execute sequence that starts from receiving");
      return false;
   }

   // assign ID
   command->SetSequenceId(idGenerator_.getNextId());
   command->SetUniqueSeed(idGenerator_.getUniqueSeed());

   // send first command
   SendCommandMessagesIfRequired(command);

   // if not completed - add to map
   if (!command->IsCompleted()) {
      RegisterUserCommand(command);
   }

   return true;
}

bool CelerClient::IsConnected() const
{
   return connection_ != nullptr && !sessionToken_.empty();
}

std::string CelerClient::userName() const
{
   return userName_;
}

void CelerClient::RegisterUserCommand(const std::shared_ptr<BaseCelerCommand>& command)
{
   activeCommands_.emplace(command->GetSequenceId(), command);
}

void CelerClient::onTimerRestart()
{
   heartbeatTimer_->start();
}

std::string CelerClient::userId() const
{
   return userId_.value;
}

const QString& CelerClient::userType() const
{
   return userType_;
}

CelerClient::CelerUserType CelerClient::celerUserType() const
{
   return celerUserType_;
}

std::unordered_set<std::string> CelerClient::GetSubmittedAuthAddressSet() const
{
   return submittedAuthAddressSet_;
}

bool CelerClient::SetSubmittedAuthAddressSet(const std::unordered_set<std::string>& addressSet)
{
   submittedAuthAddressSet_ = addressSet;

   std::string stringValue = SetToString(addressSet);

   submittedAuthAddressListProperty_.value = stringValue;
   auto command = std::make_shared<CelerSetUserPropertySequence>(logger_, userName_
      , submittedAuthAddressListProperty_);

   return ExecuteSequence(command);
}

bool CelerClient::IsCCAddressSubmitted(const std::string &address) const
{
   const auto it = submittedCCAddressSet_.find(address);
   if (it != submittedCCAddressSet_.end()) {
      return true;
   }
   return false;
}

bool CelerClient::SetCCAddressSubmitted(const std::string &address)
{
   if (IsCCAddressSubmitted(address)) {
      return true;
   }

   submittedCCAddressSet_.insert(address);
   submittedCCAddressListProperty_.value = SetToString(submittedCCAddressSet_);
   const auto command = std::make_shared<CelerSetUserPropertySequence>(logger_, userName_
      , submittedCCAddressListProperty_);

   return ExecuteSequence(command);
}

std::string CelerClient::SetToString(const std::unordered_set<std::string> &set)
{
   std::string stringValue;

   if (set.empty()) {
      return "";
   }

   for (const auto& address : set ) {
      stringValue.append(address);
      stringValue.append(";");
   }

   // remove last column
   stringValue.resize(stringValue.size() - 1);

   return stringValue;
}

void CelerClient::UpdateSetFromString(const std::string& value, std::unordered_set<std::string> &set)
{
   set.clear();

   if (value.empty()) {
      return;
   }

   size_t start = 0;
   size_t end = 0;

   const char *data = value.c_str();

   while (end < value.length()) {
      if (data[end] == ';') {
         AddToSet( {&data[start], end - start}, set);
         start = end + 1;
      }
      end += 1;
   }
   AddToSet( {&data[start], end - start}, set);
}

void CelerClient::AddToSet(const std::string& address, std::unordered_set<std::string> &set)
{
   auto it = set.find(address);
   if (it == set.end()) {
      set.emplace(address);
   }
}

bool CelerClient::tradingAllowed() const
{
   return (bitcoinParticipant_.value == "true");
}
