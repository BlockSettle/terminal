#ifndef CLIENTPARTYLOGIC_H
#define CLIENTPARTYLOGIC_H

#include <QObject>
#include <memory>
#include <google/protobuf/message.h>

#include "ChatProtocol/ClientPartyModel.h"
#include "ChatProtocol/ClientDBService.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/UserPublicKeyInfo.h"

#include <google/protobuf/message.h>

namespace spdlog
{
   class logger;
}

namespace Chat
{
   enum class ClientPartyLogicError
   {
      NonexistentClientStatusChanged,
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

      Chat::ClientPartyModelPtr clientPartyModelPtr() const { return clientPartyModelPtr_; }
      void setClientPartyModelPtr(Chat::ClientPartyModelPtr val) { clientPartyModelPtr_ = val; }

      void handlePartiesFromWelcomePacket(const WelcomeResponse& msg);

      void createPrivateParty(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName);
      void createPrivatePartyFromPrivatePartyRequest(const ChatUserPtr& currentUserPtr, const PrivatePartyRequest& privatePartyRequest);

   signals:
      void error(const Chat::ClientPartyLogicError& errorCode, const std::string& what = "");
      void partyModelChanged();
      void sendPartyMessagePacket(const google::protobuf::Message& message);
      void privatePartyCreated(const std::string& partyId);
      void privatePartyAlreadyExist(const std::string& partyId);
      void deletePrivateParty(const std::string& partyId);
      void userPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);

   public slots:
      void onUserStatusChanged(const StatusChanged& statusChanged);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void loggedOutFromServer();
      void updateModelAndRefreshPartyDisplayNames();

   private slots:
      void handleLocalErrors(const Chat::ClientPartyLogicError& errorCode, const std::string& what);
      void handlePartyInserted(const Chat::PartyPtr& partyPtr);
      void clientPartyDisplayNameChanged(const std::string& partyId);

      void onRecipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList);
      void onRecipientKeysUnchanged();

   private:
      LoggerPtr loggerPtr_;
      ClientPartyModelPtr clientPartyModelPtr_;
      ClientDBServicePtr clientDBServicePtr_;
   };

   using ClientPartyLogicPtr = std::shared_ptr<ClientPartyLogic>;
}

Q_DECLARE_METATYPE(Chat::ClientPartyLogicError)

#endif // CLIENTPARTYLOGIC_H
