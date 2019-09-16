#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <google/protobuf/any.pb.h>

#include "ChatProtocol/ChatClientLogic.h"

#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "UserHasher.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

#include "chat.pb.h"

using namespace Chat;

const auto kEmailRegex = QStringLiteral(R"(^\S+@\S+\.\S+$)");

ChatClientLogic::ChatClientLogic()
{
   qRegisterMetaType<DataConnectionListener::DataConnectionError>();
   qRegisterMetaType<Chat::ChatClientLogicError>();
   qRegisterMetaType<Chat::ClientPartyLogicPtr>();
   qRegisterMetaType<Chat::ChatUserPtr>();

   connect(this, &ChatClientLogic::chatClientError, this, &ChatClientLogic::handleLocalErrors);
}

ChatClientLogic::~ChatClientLogic()
{

}

void ChatClientLogic::initDbDone()
{
   connect(currentUserPtr_.get(), &ChatUser::userNameChanged, this, &ChatClientLogic::chatUserUserNameChanged);

   setClientPartyLogicPtr(std::make_shared<ClientPartyLogic>(loggerPtr_, clientDBServicePtr_, this));
   connect(clientPartyLogicPtr_.get(), &ClientPartyLogic::partyModelChanged, this, &ChatClientLogic::partyModelChanged);
   connect(clientPartyLogicPtr_.get(), &ClientPartyLogic::privatePartyCreated, this, &ChatClientLogic::privatePartyCreated);
   connect(clientPartyLogicPtr_.get(), &ClientPartyLogic::privatePartyAlreadyExist, this, &ChatClientLogic::privatePartyAlreadyExist);
   connect(clientPartyLogicPtr_.get(), &ClientPartyLogic::deletePrivateParty, this, &ChatClientLogic::DeletePrivateParty);
   connect(this, &ChatClientLogic::clientLoggedOutFromServer, clientPartyLogicPtr_.get(), &ClientPartyLogic::loggedOutFromServer);

   sessionKeyHolderPtr_ = std::make_shared<SessionKeyHolder>(loggerPtr_, this);
   clientConnectionLogicPtr_ = std::make_shared<ClientConnectionLogic>(
      clientPartyLogicPtr_, 
      applicationSettingsPtr_, 
      clientDBServicePtr_, 
      loggerPtr_, 
      cryptManagerPtr_, 
      sessionKeyHolderPtr_,
      this);
   clientConnectionLogicPtr_->setCurrentUserPtr(currentUserPtr_);
   connect(this, &ChatClientLogic::dataReceived, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::onDataReceived);
   connect(this, &ChatClientLogic::connected, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::onConnected);
   connect(this, &ChatClientLogic::disconnected, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::onDisconnected);
   connect(this, qOverload<DataConnectionListener::DataConnectionError>(&ChatClientLogic::error),
      clientConnectionLogicPtr_.get(), qOverload<DataConnectionListener::DataConnectionError>(&ClientConnectionLogic::onError));
   connect(this, &ChatClientLogic::messagePacketSent, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::messagePacketSent);
   connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::sendPacket, this, &ChatClientLogic::sendPacket);
   connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::closeConnection, this, &ChatClientLogic::onCloseConnection);
   connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::searchUserReply, this, &ChatClientLogic::searchUserReply);
   connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::properlyConnected, this, &ChatClientLogic::properlyConnected);

   // close connection from callback
   connect(this, &ChatClientLogic::disconnected, this, &ChatClientLogic::onCloseConnection);

   emit initDone();
}

void ChatClientLogic::Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettingsPtr, const LoggerPtr& loggerPtr)
{
   if (connectionManagerPtr_) {
      // already initialized
      emit chatClientError(ChatClientLogicError::ConnectionAlreadyInitialized);
      return;
   }

   userHasherPtr_ = std::make_shared<UserHasher>();
   cryptManagerPtr_ = std::make_shared<CryptManager>(loggerPtr);

   connectionManagerPtr_ = connectionManagerPtr;
   applicationSettingsPtr_ = appSettingsPtr;
   loggerPtr_ = loggerPtr;

   clientDBServicePtr_ = std::make_shared<ClientDBService>();
   connect(clientDBServicePtr_.get(), &ClientDBService::initDone, this, &ChatClientLogic::initDbDone);

   currentUserPtr_ = std::make_shared<ChatUser>();
   currentUserPtr_->setPrivateKey(SecureBinaryData(
      applicationSettingsPtr_->GetAuthKeys().first.data(),
      applicationSettingsPtr_->GetAuthKeys().first.size()
   ));
   currentUserPtr_->setPublicKey(BinaryData(
      applicationSettingsPtr_->GetAuthKeys().second.data(),
      applicationSettingsPtr_->GetAuthKeys().second.size()
   ));

   clientDBServicePtr_->Init(loggerPtr, appSettingsPtr, currentUserPtr_, cryptManagerPtr_);
}

void ChatClientLogic::LoginToServer(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb& cb)
{
   if (connectionPtr_) {
      loggerPtr_->error("[ChatClientLogic::LoginToServer] connecting with not purged connection");

      emit chatClientError(ChatClientLogicError::ConnectionAlreadyUsed);
      connectionPtr_->closeConnection();
      connectionPtr_.reset();
   }

   connectionPtr_ = connectionManagerPtr_->CreateZMQBIP15XDataConnection();
   connectionPtr_->setCBs(cb);

   currentUserPtr_->setUserName(userHasherPtr_->deriveKey(email));
   clientPartyModelPtr()->setOwnUserName(currentUserPtr_->userName());

   if (!connectionPtr_->openConnection(this->getChatServerHost(), this->getChatServerPort(), this))
   {
      loggerPtr_->error("[ChatClientLogic::LoginToServer] failed to open ZMQ data connection");
      connectionPtr_.reset();
      clientPartyModelPtr()->setOwnUserName({});

      emit chatClientError(ChatClientLogicError::ZmqDataConnectionFailed);
      emit clientLoggedOutFromServer();
      return;
   }
}

std::string ChatClientLogic::getChatServerHost() const
{
   return applicationSettingsPtr_->get<std::string>(ApplicationSettings::chatServerHost);
}

std::string ChatClientLogic::getChatServerPort() const
{
   return applicationSettingsPtr_->get<std::string>(ApplicationSettings::chatServerPort);
}

void ChatClientLogic::OnDataReceived(const std::string& data)
{
   emit dataReceived(data);
}

void ChatClientLogic::OnConnected(void)
{
   emit connected();
}

void ChatClientLogic::OnDisconnected(void)
{
   emit disconnected();
}

void ChatClientLogic::OnError(DataConnectionListener::DataConnectionError dataConnectionError)
{
   QString errorString = QStringLiteral("DataConnectionError: %1").arg(dataConnectionError);
   emit chatClientError(ChatClientLogicError::ZmqDataConnectionFailed, errorString.toStdString());
   emit error(dataConnectionError);
   OnDisconnected();
}

void ChatClientLogic::sendPacket(const google::protobuf::Message& message)
{
   auto packetString = ProtobufUtils::pbMessageToString(message);

   google::protobuf::Any any;
   any.ParseFromString(packetString);

   loggerPtr_->debug("[ChatClientLogic::sendPacket] send: {}", ProtobufUtils::toJsonReadable(any));

   if (!connectionPtr_->isActive())
   {
      loggerPtr_->error("[ChatClientLogic::sendPacket] Connection is not alive!");
      return;
   }

   if (!connectionPtr_->send(packetString))
   {
      loggerPtr_->error("[ChatClientLogic::sendPacket] Failed to send packet!");
      return;
   }

   if (any.Is<PartyMessagePacket>())
   {
      // update message state to SENT value
      PartyMessagePacket partyMessagePacket;
      any.UnpackTo(&partyMessagePacket);

      emit messagePacketSent(partyMessagePacket.message_id());
   }
}

void ChatClientLogic::LogoutFromServer()
{
   if (!connectionPtr_)
   {
      emit clientLoggedOutFromServer();
      return;
   }

   LogoutRequest logoutRequest;
   sendPacket(logoutRequest);
}

void ChatClientLogic::onCloseConnection()
{
   if (nullptr == connectionPtr_)
   {
      return;
   }

   connectionPtr_.reset();
   emit clientLoggedOutFromServer();
}

void ChatClientLogic::SendPartyMessage(const std::string& partyId, const std::string& data)
{
   ClientPartyPtr clientPartyPtr = clientPartyLogicPtr_->clientPartyModelPtr()->getClientPartyById(partyId);

   if (nullptr == clientPartyPtr)
   {
      emit chatClientError(ChatClientLogicError::ClientPartyNotExist, partyId);
      return;
   }

   clientConnectionLogicPtr_->prepareAndSendMessage(clientPartyPtr, data);
}

void ChatClientLogic::handleLocalErrors(const ChatClientLogicError& errorCode, const std::string& what)
{
   loggerPtr_->debug("[ChatClientLogic::handleLocalErrors] Error: {}, what: {}", (int)errorCode, what);
}

void ChatClientLogic::SetMessageSeen(const std::string& partyId, const std::string& messageId)
{
   ClientPartyPtr clientPartyPtr = clientPartyLogicPtr_->clientPartyModelPtr()->getClientPartyById(partyId);

   if (nullptr == clientPartyPtr)
   {
      emit chatClientError(ChatClientLogicError::ClientPartyNotExist, partyId);
      return;
   }

   clientConnectionLogicPtr_->setMessageSeen(clientPartyPtr, messageId);
}

void ChatClientLogic::RequestPrivateParty(const std::string& remoteUserName)
{
   clientPartyLogicPtr_->createPrivateParty(currentUserPtr_, remoteUserName);
}

void ChatClientLogic::privatePartyCreated(const std::string& partyId)
{
   clientConnectionLogicPtr_->prepareRequestPrivateParty(partyId);
}

void ChatClientLogic::privatePartyAlreadyExist(const std::string& partyId)
{
   clientConnectionLogicPtr_->prepareRequestPrivateParty(partyId);
}

void ChatClientLogic::RejectPrivateParty(const std::string& partyId)
{
   clientConnectionLogicPtr_->rejectPrivateParty(partyId);
}

void ChatClientLogic::AcceptPrivateParty(const std::string& partyId)
{
   clientConnectionLogicPtr_->acceptPrivateParty(partyId);
}

void ChatClientLogic::DeletePrivateParty(const std::string& partyId)
{
   // set party state as rejected
   clientConnectionLogicPtr_->rejectPrivateParty(partyId);

   // then delete local
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   PartyPtr partyPtr = clientPartyModelPtr->getPartyById(partyId);

   if (nullptr == partyPtr)
   {
      emit chatClientError(ChatClientLogicError::PartyNotExist, partyId);
      return;
   }

   // TODO: remove party and all messages

   // if party in rejected state then remove recipients public keys, we don't need them anymore
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyId);
   if (clientPartyPtr && clientPartyPtr->isPrivateStandard())
   {
      PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr_->userName());
      clientDBServicePtr_->deleteRecipientsKeys(recipients);
   }

   clientPartyModelPtr->removeParty(partyPtr);
}

void ChatClientLogic::SearchUser(const std::string& userHash, const std::string& searchId)
{
   QRegularExpression re(kEmailRegex);
   if (!re.isValid())
   {
      return;
   }
   QRegularExpressionMatch match = re.match(QString::fromStdString(userHash));

   std::string stringToSearch;
   if (match.hasMatch())
   {
      stringToSearch = userHasherPtr_->deriveKey(userHash);
   }
   else
   {
      stringToSearch = userHash;
   }

   clientConnectionLogicPtr_->searchUser(stringToSearch, searchId);
}

void ChatClientLogic::AcceptNewPublicKeys(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList)
{
   PartyRecipientsPtrList recipientsToUpdate;
   std::vector<std::string> partiesToCheckUnsentMessages;

   for (const auto& userPkPtr : userPublicKeyInfoList)
   {
      // update loaded user key
      const std::string userHash = userPkPtr->user_hash().toStdString();
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr()->getClientPartyByUserHash(userHash);
      if (clientPartyPtr && clientPartyPtr->isPrivateStandard())
      {
         PartyRecipientPtr existingRecipient = clientPartyPtr->getRecipient(userHash);

         if (existingRecipient)
         {
            existingRecipient->setPublicKey(userPkPtr->newPublicKey());
            existingRecipient->setPublicKeyTime(userPkPtr->newPublicKeyTime());

            recipientsToUpdate.push_back(existingRecipient);

            // force clear session keys
            sessionKeyHolderPtr_->clearSessionForUser(existingRecipient->userName());

            // save party id for handling later
            partiesToCheckUnsentMessages.push_back(clientPartyPtr->id());
         }
      }
   }

   clientDBServicePtr_->updateRecipientKeys(recipientsToUpdate);

   // after updating the keys, check if we have unsent messages
   for (const auto& partyId : partiesToCheckUnsentMessages)
   {
      clientDBServicePtr_->checkUnsentMessages(partyId);
   }

   clientPartyLogicPtr_->updateModelAndRefreshPartyDisplayNames();
}

void ChatClientLogic::DeclineNewPublicKeys(const UserPublicKeyInfoList& userPublicKeyInfoList)
{
   // remove all parties for declined user
   for (const auto& userPkPtr : userPublicKeyInfoList)
   {
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr()->getClientPartyByUserHash(userPkPtr->user_hash().toStdString());

      if (clientPartyPtr && clientPartyPtr->isPrivateStandard())
      {
         DeletePrivateParty(clientPartyPtr->id());
      }
   }

   clientPartyLogicPtr_->updateModelAndRefreshPartyDisplayNames();
}