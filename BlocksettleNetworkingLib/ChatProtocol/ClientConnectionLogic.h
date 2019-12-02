/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CLIENTCONNECTIONLOGIC_H
#define CLIENTCONNECTIONLOGIC_H

#include <memory>
#include <QObject>

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
   class PartyMessageOfflineRequest;
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
      explicit ClientConnectionLogic(ClientPartyLogicPtr clientPartyLogicPtr, ApplicationSettingsPtr appSettings,
                                     ClientDBServicePtr clientDBServicePtr, LoggerPtr loggerPtr,
                                     CryptManagerPtr cryptManagerPtr,
                                     SessionKeyHolderPtr sessionKeyHolderPtr, QObject* parent = nullptr);

      ChatUserPtr currentUserPtr() const { return currentUserPtr_; }
      void setCurrentUserPtr(const ChatUserPtr& val) { currentUserPtr_ = val; }

      void prepareAndSendMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void setMessageSeen(const ClientPartyPtr& clientPartyPtr, const std::string& messageId);

      void prepareRequestPrivateParty(const std::string& partyId);
      void rejectPrivateParty(const std::string& partyId);
      void acceptPrivateParty(const std::string& partyId);
      void searchUser(const std::string& userHash, const std::string& searchId);

      void setToken(const BinaryData &token, const BinaryData &tokenSign);

   public slots:
      void onDataReceived(const std::string&);
      void onConnected();
      void onDisconnected();
      void onError(DataConnectionListener::DataConnectionError);

      void messagePacketSent(const std::string& messageId) const;

      void sessionKeysForUser(const Chat::SessionKeyDataPtr& sessionKeyDataPtr) const;
      void sessionKeysForUserFailed(const std::string& userName);

   signals:
      void sendPacket(const google::protobuf::Message& message);
      void closeConnection();
      void userStatusChanged(const Chat::ChatUserPtr& currentUserPtr, const Chat::StatusChanged& statusChanged);
      void error(const Chat::ClientConnectionLogicError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void properlyConnected();
      void searchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId);
      void deletePrivateParty(const std::string& partyId);

   private slots:
      void handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what = "", bool displayAsWarning = false) const;
      void messageLoaded(const std::string& partyId, const std::string& messageId, qint64 timestamp,
         const std::string& message, int encryptionType, const std::string& nonce, int partyMessageState);
      void unsentMessagesFound(const std::string& partyId) const;

   private:
      void prepareAndSendPublicMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void prepareAndSendPrivateMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data) const;

      void requestSessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey);
      void replySessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey);

      void handleWelcomeResponse(const WelcomeResponse& welcomeResponse);
      void handleLogoutResponse(const LogoutResponse& logoutResponse);
      void handleStatusChanged(const StatusChanged& statusChanged);
      void handlePartyMessageStateUpdate(const PartyMessageStateUpdate& partyMessageStateUpdate) const;
      void handlePartyMessagePacket(PartyMessagePacket& partyMessagePacket);
      void handlePrivatePartyRequest(const PrivatePartyRequest& privatePartyRequest);
      void handleRequestSessionKeyExchange(const RequestSessionKeyExchange& requestKeyExchange) const;
      void handleReplySessionKeyExchange(const ReplySessionKeyExchange& replyKeyExchange) const;
      void handlePrivatePartyStateChanged(const PrivatePartyStateChanged& privatePartyStateChanged);
      void handleReplySearchUser(const ReplySearchUser& replySearchUser);
      void handlePartyMessageOfflineRequest(const PartyMessageOfflineRequest& partyMessageOfflineRequest) const;

      void incomingGlobalPartyMessage(PartyMessagePacket& partyMessagePacket);
      void incomingPrivatePartyMessage(PartyMessagePacket& partyMessagePacket);
      void saveIncomingPartyMessageAndUpdateState(PartyMessagePacket& partyMessagePacket, const PartyMessageState& partyMessageState);
      void saveRecipientsKeys(const ClientPartyPtr& clientPartyPtr) const;

      LoggerPtr   loggerPtr_;
      ChatUserPtr currentUserPtr_;
      ApplicationSettingsPtr appSettings_;
      ClientPartyLogicPtr clientPartyLogicPtr_;
      ClientDBServicePtr clientDBServicePtr_;
      SessionKeyHolderPtr sessionKeyHolderPtr_;
      CryptManagerPtr cryptManagerPtr_;
      BinaryData token_;
      BinaryData tokenSign_;
   };

   using ClientConnectionLogicPtr = std::shared_ptr<ClientConnectionLogic>;
}

Q_DECLARE_METATYPE(Chat::SearchUserReplyList)

#endif // CLIENTCONNECTIONLOGIC_H
