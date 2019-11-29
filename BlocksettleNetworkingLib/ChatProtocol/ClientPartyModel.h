/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CLIENTPARTYMODEL_H
#define CLIENTPARTYMODEL_H

#include <QMetaType>
#include <functional>
#include <memory>
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
      IdPartyList getIdPrivatePartyListBySubType(const PartySubType& partySubType = STANDARD);

      ClientPartyPtrList getClientPartyListFromIdPartyList(const IdPartyList& idPartyList);
      ClientPartyPtrList getClientPartyListForRecipient(const IdPartyList& idPartyList, const std::string& recipientUserHash);
      ClientPartyPtrList getStandardPrivatePartyListForRecipient(const std::string& recipientUserHash);
      ClientPartyPtrList getOtcPrivatePartyListForRecipient(const std::string& recipientUserHash);
      ClientPartyPtrList getClientPartyListByCreatorHash(const std::string& creatorHash);

      ClientPartyPtr getStandardPartyForUsers(const std::string& firstUserHash, const std::string& secondUserHash);
      ClientPartyPtr getOtcPartyForUsers(const std::string& firstUserHash, const std::string& secondUserHash);
      static ClientPartyPtrList getClientPartyForRecipients(const ClientPartyPtrList& clientPartyPtrList, const std::string& firstUserHash, const std::string& secondUserHash);

      ClientPartyPtr getClientPartyById(const std::string& party_id);

      const std::string& ownUserName() const { return ownUserName_; }
      void setOwnUserName(const std::string& val) { ownUserName_ = val; }
      PrivatePartyState deducePrivatePartyStateForUser(const std::string& userName);

      CelerClient::CelerUserType ownCelerUserType() const { return ownCelerUserType_; }
      void setOwnCelerUserType(CelerClient::CelerUserType val) { ownCelerUserType_ = val; }
   signals:
      void error(const Chat::ClientPartyModelError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void clientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
      void messageArrived(const Chat::MessagePtrList& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, int party_message_state);
      void partyStateChanged(const std::string& partyId);
      void clientPartyDisplayNameChanged(const std::string& partyId);
      void userPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);
      void otcPrivatePartyReady(const Chat::ClientPartyPtr& clientPartyPtr);

   private slots:
      void handleLocalErrors(const Chat::ClientPartyModelError& errorCode, const std::string& what = "", bool displayAsWarning = false) const;
      void handlePartyInserted(const Chat::PartyPtr& partyPtr);
      void handlePartyRemoved(const Chat::PartyPtr& partyPtr);
      void handlePartyStatusChanged(const Chat::ClientStatus& clientStatus);
      void handlePartyStateChanged(const std::string& partyId);
      void handleDisplayNameChanged();
      
   private:
      ClientPartyPtr castToClientPartyPtr(const PartyPtr& partyPtr);
      static ClientPartyPtr getFirstClientPartyForPartySubType(const ClientPartyPtrList& clientPartyPtrList, 
         const std::string& firstUserHash, const std::string& secondUserHash, const PartySubType& partySubType = STANDARD);
      std::string ownUserName_;
      CelerClient::CelerUserType ownCelerUserType_ = bs::network::UserType::Undefined;
   };

   using ClientPartyModelPtr = std::shared_ptr<ClientPartyModel>;

}

Q_DECLARE_METATYPE(Chat::PrivatePartyState)
Q_DECLARE_METATYPE(Chat::ClientPartyModelError)

#endif // CLIENTPARTYMODEL_H
