#ifndef CLIENTCONNECTIONLOGIC_H
#define CLIENTCONNECTIONLOGIC_H

#include <memory>
#include <QObject>
#include <google/protobuf/message.h>

#include "DataConnectionListener.h"
#include "ApplicationSettings.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientDBService.h"
#include "ChatProtocol/SessionKeyHolder.h"
#include "ChatProtocol/CryptManager.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   class WelcomeResponse;
   class LogoutResponse;
   class StatusChanged;
   class PartyMessageStateUpdate;
   class PartyMessagePacket;
   class PrivatePartyRequest;
   class RequestSessionKeyExchange;
   class ReplySessionKeyExchange;
   class PrivatePartyStateChanged;
   class ReplySearchUser;

   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;
   using SearchUserReplyList = std::vector<std::string>;

   enum class ClientConnectionLogicError
   {
      SendingDataToUnhandledParty,
      UnhandledPacket,
      CouldNotFindParty,
      DynamicPointerCast,
      WrongPartyRecipient,
      ParsingPacketData
   };

   class ClientConnectionLogic : public QObject
   {
      Q_OBJECT
   public:
      explicit ClientConnectionLogic(const ClientPartyLogicPtr& clientPartyLogicPtr, const ApplicationSettingsPtr& appSettings, 
         const ClientDBServicePtr& clientDBServicePtr, const LoggerPtr& loggerPtr, const Chat::CryptManagerPtr& cryptManagerPtr,
         QObject* parent = nullptr);

      Chat::ChatUserPtr currentUserPtr() const { return currentUserPtr_; }
      void setCurrentUserPtr(Chat::ChatUserPtr val) { currentUserPtr_ = val; }
      void SendPartyMessage(const std::string& partyId, const std::string& data);

      void prepareAndSendMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void setMessageSeen(const ClientPartyPtr& clientPartyPtr, const std::string& messageId);

      void prepareRequestPrivateParty(const std::string& partyId);
      void rejectPrivateParty(const std::string& partyId);
      void acceptPrivateParty(const std::string& partyId);
      void searchUser(const std::string& userHash, const std::string& searchId);

   public slots:
      void onDataReceived(const std::string&);
      void onConnected(void);
      void onDisconnected(void);
      void onError(DataConnectionListener::DataConnectionError);

      void messagePacketSent(const std::string& messageId);

      void sessionKeysForUser(const Chat::SessionKeyDataPtr& sessionKeyDataPtr);
      void sessionKeysForUserFailed(const std::string& userName);

   signals:
      void sendPacket(const google::protobuf::Message& message);
      void closeConnection();
      void userStatusChanged(const std::string& userName, const ClientStatus& clientStatus);
      void error(const Chat::ClientConnectionLogicError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void properlyConnected();
      void searchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId);

   private slots:
      void handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
         const std::string& message, const int encryptionType, const std::string& nonce, const int party_message_state);
      void unsentMessagesFound(const std::string& partyId);

   private:
      void prepareAndSendPublicMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void prepareAndSendPrivateMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);

      void requestSessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey);
      void replySessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey);

      void handleWelcomeResponse(const WelcomeResponse& welcomeResponse);
      void handleLogoutResponse(const LogoutResponse& logoutResponse);
      void handleStatusChanged(const StatusChanged& statusChanged);
      void handlePartyMessageStateUpdate(const PartyMessageStateUpdate& partyMessageStateUpdate);
      void handlePartyMessagePacket(PartyMessagePacket& partyMessagePacket);
      void handlePrivatePartyRequest(const PrivatePartyRequest& privatePartyRequest);
      void handleRequestSessionKeyExchange(const RequestSessionKeyExchange& requestKeyExchange);
      void handleReplySessionKeyExchange(const ReplySessionKeyExchange& replyKeyExchange);
      void handlePrivatePartyStateChanged(const PrivatePartyStateChanged& privatePartyStateChanged);
      void handleReplySearchUser(const ReplySearchUser& replySearchUser);

      void incomingGlobalPartyMessage(PartyMessagePacket& msg);
      void incomingPrivatePartyMessage(PartyMessagePacket& partyMessagePacket);
      void saveIncomingPartyMessageAndUpdateState(PartyMessagePacket& msg, const PartyMessageState& partyMessageState);

      LoggerPtr   loggerPtr_;
      ChatUserPtr currentUserPtr_;
      ApplicationSettingsPtr appSettings_;
      ClientPartyLogicPtr clientPartyLogicPtr_;
      ClientDBServicePtr clientDBServicePtr_;
      SessionKeyHolderPtr sessionKeyHolderPtr_;
      CryptManagerPtr cryptManagerPtr_;
   };

   using ClientConnectionLogicPtr = std::shared_ptr<ClientConnectionLogic>;
}

Q_DECLARE_METATYPE(Chat::SearchUserReplyList)

#endif // CLIENTCONNECTIONLOGIC_H
