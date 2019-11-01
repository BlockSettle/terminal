#ifndef CLIENTPARTYLOGIC_H
#define CLIENTPARTYLOGIC_H

#include <memory>
#include <google/protobuf/message.h>

#include "ChatProtocol/ClientPartyModel.h"
#include "ChatProtocol/ClientDBService.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/UserPublicKeyInfo.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   enum class ClientPartyLogicError
   {
      PartyNotExist,
      DynamicPointerCast,
      QObjectCast
   };

   class WelcomeResponse;

   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class ClientPartyLogic : public QObject
   {
      Q_OBJECT
   public:
      ClientPartyLogic(const LoggerPtr& loggerPtr, const ClientDBServicePtr& clientDBServicePtr, QObject* parent = nullptr);

      ClientPartyModelPtr clientPartyModelPtr() const { return clientPartyModelPtr_; }
      void setClientPartyModelPtr(const ClientPartyModelPtr& val) { clientPartyModelPtr_ = val; }

      void handlePartiesFromWelcomePacket(const ChatUserPtr& currentUserPtr, const WelcomeResponse& welcomeResponse) const;

      void createPrivateParty(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName, const PartySubType& partySubType = STANDARD,
         const std::string& initialMessage = "");
      void createPrivatePartyFromPrivatePartyRequest(const ChatUserPtr& currentUserPtr, const PrivatePartyRequest& privatePartyRequest);

   signals:
      void error(const Chat::ClientPartyLogicError& errorCode, const std::string& what = "");
      void partyModelChanged();
      void sendPartyMessagePacket(const google::protobuf::Message& message);
      void privatePartyCreated(const std::string& partyId);
      void privatePartyAlreadyExist(const std::string& partyId);
      void deletePrivateParty(const std::string& partyId);
      void userPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);
      void acceptOTCPrivateParty(const std::string& partyId);
      void messageArrived(const MessagePtrList& messagePtrList);

   public slots:
      void onUserStatusChanged(const Chat::ChatUserPtr& currentUserPtr, const Chat::StatusChanged& statusChanged);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void loggedOutFromServer() const;
      void updateModelAndRefreshPartyDisplayNames();

   private slots:
      void handleLocalErrors(const Chat::ClientPartyLogicError& errorCode, const std::string& what) const;
      void handlePartyInserted(const Chat::PartyPtr& partyPtr) const;
      void clientPartyDisplayNameChanged(const std::string& partyId) const;
      void handleMessageArrived(const Chat::MessagePtrList& messagePtr);

      void onRecipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList);
      void onRecipientKeysUnchanged();

   private:
      bool isPrivatePartyForUserExist(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName, const PartySubType& partySubType = STANDARD);
      LoggerPtr loggerPtr_;
      ClientPartyModelPtr clientPartyModelPtr_;
      ClientDBServicePtr clientDBServicePtr_;
   };

   using ClientPartyLogicPtr = std::shared_ptr<ClientPartyLogic>;
}

Q_DECLARE_METATYPE(Chat::ClientPartyLogicError)

#endif // CLIENTPARTYLOGIC_H
