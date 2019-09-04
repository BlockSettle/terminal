#ifndef ConnectionLogic_h__
#define ConnectionLogic_h__

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

#include "chat.pb.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;
   using SearchUserReplyList = std::vector<std::string>;

   enum class ClientConnectionLogicError
   {
      SendingDataToUnhandledParty,
      UnhandledPacket,
      CouldNotFindParty,
      DynamicPointerCast,
      WrongPartyRecipient
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
      void error(const Chat::ClientConnectionLogicError& errorCode, const std::string& what);
      void properlyConnected();
      void searchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId);

   private slots:
      void handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what = "");
      void messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
         const std::string& message, const int encryptionType, const std::string& nonce, const int party_message_state);
      void unsentMessagesFound(const std::string& partyId);

   private:
      void prepareAndSendPublicMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void prepareAndSendPrivateMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);

      void requestSessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey);
      void replySessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey);

      void handleWelcomeResponse(const google::protobuf::Message& msg);
      void handleLogoutResponse(const google::protobuf::Message& msg);
      void handleStatusChanged(const google::protobuf::Message& msg);
      void handlePartyMessageStateUpdate(const google::protobuf::Message& msg);
      void handlePartyMessagePacket(const google::protobuf::Message& msg);
      void handlePrivatePartyRequest(const google::protobuf::Message& msg);
      void handleRequestSessionKeyExchange(const google::protobuf::Message& msg);
      void handleReplySessionKeyExchange(const google::protobuf::Message& msg);
      void handlePrivatePartyStateChanged(const google::protobuf::Message& msg);
      void handleReplySearchUser(const google::protobuf::Message& msg);

      void incomingGlobalPartyMessage(const google::protobuf::Message& msg);
      void incomingPrivatePartyMessage(const google::protobuf::Message& msg);
      void saveIncomingPartyMessageAndUpdateState(const google::protobuf::Message& msg, const PartyMessageState& partyMessageState);

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

#endif // ConnectionLogic_h__
