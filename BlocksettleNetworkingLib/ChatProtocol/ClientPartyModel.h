#ifndef CLIENTPARTYMODEL_H
#define CLIENTPARTYMODEL_H

#include <QMetaType>
#include <QObject>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ChatProtocol/PartyModel.h"
#include "ChatProtocol/ClientParty.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/UserPublicKeyInfo.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   enum class ClientPartyModelError
   {
      DynamicPointerCast,
      UserNameNotFound,
      UserHashNotFound,
      PartyCreatorHashNotFound,
      QObjectCast,
      PartyNotFound
   };

   enum class PrivatePartyState
   {
      Unknown,
      Uninitialized,
      RequestedOutgoing,
      RequestedIncoming,
      Rejected,
      Initialized,
      QObjectCast
   };
   
   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class ClientPartyModel : public PartyModel
   {
      Q_OBJECT
   public:
      ClientPartyModel(const LoggerPtr& loggerPtr, QObject* parent = nullptr);
      IdPartyList getIdPartyList();
      IdPartyList getIdPrivatePartyList();
      IdPartyList getIdPrivatePartyListBySubType(const PartySubType& partySubType = PartySubType::STANDARD);

      ClientPartyPtrList getClientPartyListFromIdPartyList(const IdPartyList& idPartyList);
      ClientPartyPtrList getClientPartyListForRecipient(const IdPartyList& idPartyList, const std::string& recipientUserHash);
      ClientPartyPtrList getStandardPrivatePartyListForRecipient(const std::string& recipientUserHash);
      ClientPartyPtrList getOtcPrivatePartyListForRecipient(const std::string& recipientUserHash);
      ClientPartyPtrList getClientPartyListByCreatorHash(const std::string& creatorHash);

      ClientPartyPtr getStandardPartyForUsers(const std::string& firstUserHash, const std::string& secondUserHash);
      ClientPartyPtr getOtcPartyForUsers(const std::string& firstUserHash, const std::string& secondUserHash);
      ClientPartyPtrList getClientPartyForRecipients(const ClientPartyPtrList& clientPartyPtrList, const std::string& firstUserHash, const std::string& secondUserHash);

      ClientPartyPtr getClientPartyById(const std::string& party_id);

      const std::string& ownUserName() const { return ownUserName_; }
      void setOwnUserName(std::string val) { ownUserName_ = val; }
      PrivatePartyState deducePrivatePartyStateForUser(const std::string& userName);

      CelerClient::CelerUserType ownCelerUserType() const { return ownCelerUserType_; }
      void setOwnCelerUserType(CelerClient::CelerUserType val) { ownCelerUserType_ = val; }
   signals:
      void error(const Chat::ClientPartyModelError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void clientPartyStatusChanged(const ClientPartyPtr& clientPartyPtr);
      void messageArrived(const Chat::MessagePtrList& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
      void partyStateChanged(const std::string& partyId);
      void clientPartyDisplayNameChanged(const std::string& partyId);
      void userPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);
      void otcPrivatePartyReady(const ClientPartyPtr& clientPartyPtr);

   private slots:
      void handleLocalErrors(const Chat::ClientPartyModelError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void handlePartyInserted(const PartyPtr& partyPtr);
      void handlePartyRemoved(const PartyPtr& partyPtr);
      void handlePartyStatusChanged(const ClientStatus& clientStatus);
      void handlePartyStateChanged(const std::string& partyId);
      void handleDisplayNameChanged();
      
   private:
      ClientPartyPtr castToClientPartyPtr(const PartyPtr& partyPtr);
      ClientPartyPtr getFirstClientPartyForPartySubType(const ClientPartyPtrList& clientPartyPtrList, 
         const std::string& firstUserHash, const std::string& secondUserHash, const PartySubType& partySubType = PartySubType::STANDARD);
      std::string ownUserName_;
      CelerClient::CelerUserType ownCelerUserType_ = bs::network::UserType::Undefined;
   };

   using ClientPartyModelPtr = std::shared_ptr<ClientPartyModel>;

}

Q_DECLARE_METATYPE(Chat::PrivatePartyState)
Q_DECLARE_METATYPE(Chat::ClientPartyModelError)

#endif // CLIENTPARTYMODEL_H
