#include <QtDebug>

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

namespace Chat
{

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

      clientConnectionLogicPtr_ = std::make_shared<ClientConnectionLogic>(clientPartyLogicPtr_, applicationSettingsPtr_, clientDBServicePtr_, loggerPtr_, cryptManagerPtr_, this);
      clientConnectionLogicPtr_->setCurrentUserPtr(currentUserPtr_);
      connect(this, &ChatClientLogic::dataReceived, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::onDataReceived);
      connect(this, &ChatClientLogic::connected, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::onConnected);
      connect(this, &ChatClientLogic::disconnected, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::onDisconnected);
      connect(this, qOverload<DataConnectionListener::DataConnectionError>(&ChatClientLogic::error),
         clientConnectionLogicPtr_.get(), qOverload<DataConnectionListener::DataConnectionError>(&ClientConnectionLogic::onError));
      connect(this, &ChatClientLogic::messagePacketSent, clientConnectionLogicPtr_.get(), &ClientConnectionLogic::messagePacketSent);

      connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::sendPacket, this, &ChatClientLogic::sendPacket);
      connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::closeConnection, this, &ChatClientLogic::onCloseConnection);

      // close connection from callback
      connect(this, &ChatClientLogic::disconnected, this, &ChatClientLogic::onCloseConnection);

      // TODO: remove
      connect(clientConnectionLogicPtr_.get(), &ClientConnectionLogic::testProperlyConnected, this, &ChatClientLogic::testProperlyConnected);

      emit initDone();
   }

   void ChatClientLogic::Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettingsPtr, const LoggerPtr& loggerPtr)
   {
      qDebug() << "ChatClientLogic::Init Thread ID:" << this->thread()->currentThreadId();

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
         loggerPtr_->error("[ChatClientLogic::{}] connecting with not purged connection", __func__);

         emit chatClientError(ChatClientLogicError::ConnectionAlreadyUsed);
      }

      connectionPtr_ = connectionManagerPtr_->CreateZMQBIP15XDataConnection();
      connectionPtr_->setCBs(cb);

      if (!connectionPtr_->openConnection(this->getChatServerHost(), this->getChatServerPort(), this))
      {
         loggerPtr_->error("[ChatClientLogic::{}] failed to open ZMQ data connection", __func__);
         connectionPtr_.reset();

         emit chatClientError(ChatClientLogicError::ZmqDataConnectionFailed);
         emit clientLoggedOutFromServer();
         return;
      }

      currentUserPtr_->setUserName(userHasherPtr_->deriveKey(email));

//    currentJwt_ = jwt;
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
      emit error(dataConnectionError);
   }

   void ChatClientLogic::sendPacket(const google::protobuf::Message& message)
   {
      qDebug() << "ChatClientLogic::sendRequestPacket Thread ID:" << this->thread()->currentThreadId();

      loggerPtr_->debug("send: {}", ProtobufUtils::toJsonCompact(message));

      if (!connectionPtr_->isActive())
      {
         loggerPtr_->error("[ChatClientLogic::{}] Connection is not alive!", __func__);
         return;
      }

      auto packetString = ProtobufUtils::pbMessageToString(message);

      if (!connectionPtr_->send(packetString))
      {
         loggerPtr_->error("[ChatClientLogic::{}] Failed to send packet!", __func__);
         return;
      }

      google::protobuf::Any any;
      any.PackFrom(message);

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

   // TODO: remove
   void ChatClientLogic::testProperlyConnected()
   {
      SendPartyMessage("Global", "test");
      //RequestPrivateParty("ds7n8iy8fdsy");
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

}
