/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BaseCelerClient.h"

#include "ConnectionManager.h"
#include "DataConnection.h"
#include "StringUtils.h"

#include "CelerGetUserIdSequence.h"
#include "CelerGetUserPropertySequence.h"
#include "CelerLoadUserInfoSequence.h"
#include "CelerLoginSequence.h"
#include "CelerPropertiesDefinitions.h"
#include "CelerSetUserPropertySequence.h"

#include "NettyCommunication.pb.h"

using namespace com::celertech::baseserver::communication::protobuf;

BaseCelerClient::BaseCelerClient(const std::shared_ptr<spdlog::logger> &logger, bool userIdRequired, bool useRecvTimer)
   : logger_(logger)
   , userId_(CelerUserProperties::UserIdPropertyName)
   , submittedAuthAddressListProperty_(CelerUserProperties::SubmittedBtcAuthAddressListPropertyName)
   , submittedCCAddressListProperty_(CelerUserProperties::SubmittedCCAddressListPropertyName)
   , userIdRequired_(userIdRequired)
   , serverNotAvailable_(false)
{
   timerSendHb_ = new QTimer(this);

   if (useRecvTimer) {
      timerRecvHb_ = new QTimer(this);
      connect(timerRecvHb_, &QTimer::timeout, this, &BaseCelerClient::onRecvHbTimeout);
   }

   connect(timerSendHb_, &QTimer::timeout, this, &BaseCelerClient::onSendHbTimeout);

   connect(this, &BaseCelerClient::closingConnection, this, &BaseCelerClient::CloseConnection, Qt::QueuedConnection);
   RegisterDefaulthandlers();

   celerUserType_ = CelerUserType::Undefined;
}

bool BaseCelerClient::SendLogin(const std::string& login, const std::string& email, const std::string& password)
{
   // create user login sequence
   sessionToken_.clear();

   celerUserType_ = CelerUserType::Undefined;

   auto loginSequence = std::make_shared<CelerLoginSequence>(logger_, login, password);
   auto onLoginSuccess = [this, login, email](const std::string& sessionToken, std::chrono::seconds heartbeatInterval) {
     loginSuccessCallback(login, email, sessionToken, heartbeatInterval);
   };
   auto onLoginFailed = [this](const std::string& errorMessage) {
     loginFailedCallback(errorMessage);
   };
   loginSequence->SetCallbackFunctions(onLoginSuccess, onLoginFailed);

   AddInternalSequence(loginSequence);

   // BsProxy will wait when Celer connects and will queue requests before that
   OnConnected();

   return true;
}

void BaseCelerClient::loginSuccessCallback(const std::string& userName, const std::string& email, const std::string& sessionToken
   , std::chrono::seconds heartbeatInterval)
{
   logger_->debug("[CelerClient::loginSuccessCallback] logged in as {}", userName);
   userName_ = userName;
   email_ = bs::toLower(email);
   sessionToken_ = sessionToken;
   heartbeatInterval_ = heartbeatInterval;
   serverNotAvailable_ = false;
   idGenerator_.setUserName(userName);

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

   QMetaObject::invokeMethod(this, [this] {
      timerSendHb_->setInterval(heartbeatInterval_);
      timerSendHb_->start();
   });

   if (timerRecvHb_) {
      // If there is nothing received for 45 seconds channel will be closed.
      // Celer will also send heartbeats but only when idle.
      timerRecvHb_->setInterval(heartbeatInterval_ + std::chrono::seconds(15));
      timerRecvHb_->start();
   }
}

void BaseCelerClient::loginFailedCallback(const std::string& errorMessage)
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

void BaseCelerClient::AddInternalSequence(const std::shared_ptr<BaseCelerCommand>& commandSequence)
{
   internalCommands_.emplace(commandSequence);
}

void BaseCelerClient::CloseConnection()
{
   QMetaObject::invokeMethod(this, [this] {
      timerSendHb_->stop();
   });

   if (timerRecvHb_) {
      timerRecvHb_->stop();
   }

   if (!sessionToken_.empty()) {
      sessionToken_.clear();
      emit OnConnectionClosed();
   }
}

void BaseCelerClient::OnDataReceived(CelerAPI::CelerMessageType messageType, const std::string& data)
{
   if (timerRecvHb_ && timerRecvHb_->isActive()) {
      timerRecvHb_->start();
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

         if (command->OnMessage({messageType, data}) ) {
            SendCommandMessagesIfRequired(command);
         }

         if (command->IsCompleted()) {
            internalCommands_.pop();
            command->FinishSequence();
         }
         return;
      } else {
         logger_->debug("[CelerClient::OnDataReceived] internal command {} not waiting for message {}"
            , command->GetCommandName(), CelerAPI::GetMessageClass(messageType));
         break;
      }
   }

   auto handlerIt = messageHandlersMap_.find(messageType);
   if (handlerIt != messageHandlersMap_.end()) {
      if (handlerIt->second(data)) {
         return;
      }
      logger_->debug("[CelerClient::OnDataReceived] handler rejected message of type {}.", CelerAPI::GetMessageClass(messageType));
   } else {
      logger_->debug("[CelerClient::OnDataReceived] ignore message {}", CelerAPI::GetMessageClass(messageType));
   }
}

void BaseCelerClient::SendCommandMessagesIfRequired(const std::shared_ptr<BaseCelerCommand>& command)
{
   while (!(command->IsCompleted() || command->IsWaitingForData())) {
      auto message = command->GetNextDataToSend();
      sendMessage(message.messageType, message.messageData);
   }
}

bool BaseCelerClient::sendMessage(CelerAPI::CelerMessageType messageType, const std::string& data)
{
   QMetaObject::invokeMethod(this, [this] {
      // reset heartbeat interval
      if (timerSendHb_->isActive()) {
         timerSendHb_->start();
      }
   });

   onSendData(messageType, data);
   return true;
}

void BaseCelerClient::OnConnected()
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

void BaseCelerClient::OnDisconnected()
{
//   commandsQueueType{}.swap(internalCommands_);
   emit closingConnection();
}

void BaseCelerClient::OnError(DataConnectionListener::DataConnectionError errorCode)
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

void BaseCelerClient::RegisterDefaulthandlers()
{
   RegisterHandler(CelerAPI::HeartbeatType, [this](const std::string& data) { return this->onHeartbeat(data); });
   RegisterHandler(CelerAPI::SingleResponseMessageType, [this](const std::string& data) { return this->onSingleMessage(data); });
   RegisterHandler(CelerAPI::ExceptionResponseMessageType, [this](const std::string& data) { return this->onExceptionResponse(data); });
   RegisterHandler(CelerAPI::MultiResponseMessageType, [this](const std::string& data) { return this->onMultiMessage(data); });
}

bool BaseCelerClient::RegisterHandler(CelerAPI::CelerMessageType messageType, const message_handler& handler)
{
   auto it = messageHandlersMap_.find(messageType);
   if (it != messageHandlersMap_.end()) {
      logger_->error("[CelerClient::RegisterHandler] handler for message {} already exists", messageType);
      return false;
   }

   messageHandlersMap_.emplace(messageType, handler);

   return true;
}

bool BaseCelerClient::onHeartbeat(const std::string& message)
{
   Heartbeat response;

   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onHeartbeat] failed to parse message");
      return false;
   }

   return true;
}

bool BaseCelerClient::onSingleMessage(const std::string& message)
{
   SingleResponseMessage response;
   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onSingleMessage] failed to parse SingleResponseMessage");
      return false;
   }

   return SendDataToSequence(response.clientrequestid(), CelerAPI::SingleResponseMessageType, message);
}

bool BaseCelerClient::onExceptionResponse(const std::string& message)
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

bool BaseCelerClient::onMultiMessage(const std::string& message)
{
   MultiResponseMessage response;
   if (!response.ParseFromString(message)) {
      logger_->error("[CelerClient::onMultiMessage] failed to parse MultiResponseMessage");
      return false;
   }

   return SendDataToSequence(response.clientrequestid(), CelerAPI::MultiResponseMessageType, message);
}

bool BaseCelerClient::SendDataToSequence(const std::string& sequenceId, CelerAPI::CelerMessageType messageType, const std::string& message)
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

void BaseCelerClient::onSendHbTimeout()
{
   Heartbeat heartbeat;
   sendMessage(CelerAPI::HeartbeatType, heartbeat.SerializeAsString());
}

void BaseCelerClient::onRecvHbTimeout()
{
   OnDisconnected();
}

bool BaseCelerClient::ExecuteSequence(const std::shared_ptr<BaseCelerCommand>& command)
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

bool BaseCelerClient::IsConnected() const
{
   return !sessionToken_.empty();
}

void BaseCelerClient::RegisterUserCommand(const std::shared_ptr<BaseCelerCommand>& command)
{
   activeCommands_.emplace(command->GetSequenceId(), command);
}

void BaseCelerClient::recvData(CelerAPI::CelerMessageType messageType, const std::string &data)
{
   OnDataReceived(messageType, data);
}

const std::string& BaseCelerClient::userId() const
{
   return userId_.value;
}

std::unordered_set<std::string> BaseCelerClient::GetSubmittedAuthAddressSet() const
{
   return submittedAuthAddressSet_;
}

bool BaseCelerClient::SetSubmittedAuthAddressSet(const std::unordered_set<std::string>& addressSet)
{
   submittedAuthAddressSet_ = addressSet;

   std::string stringValue = SetToString(addressSet);

   submittedAuthAddressListProperty_.value = stringValue;
   auto command = std::make_shared<CelerSetUserPropertySequence>(logger_, userName_
      , submittedAuthAddressListProperty_);

   return ExecuteSequence(command);
}

bool BaseCelerClient::IsCCAddressSubmitted(const std::string &address) const
{
   const auto it = submittedCCAddressSet_.find(address);
   if (it != submittedCCAddressSet_.end()) {
      return true;
   }
   return false;
}

bool BaseCelerClient::SetCCAddressSubmitted(const std::string &address)
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

std::string BaseCelerClient::SetToString(const std::unordered_set<std::string> &set)
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

void BaseCelerClient::UpdateSetFromString(const std::string& value, std::unordered_set<std::string> &set)
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

void BaseCelerClient::AddToSet(const std::string& address, std::unordered_set<std::string> &set)
{
   auto it = set.find(address);
   if (it == set.end()) {
      set.emplace(address);
   }
}

bool BaseCelerClient::tradingAllowed() const
{
   return (IsConnected() && (bitcoinParticipant_.value == "true"));
}
