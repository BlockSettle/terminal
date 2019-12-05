/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CLIENTDBSERVICE_H
#define CLIENTDBSERVICE_H

#include <QObject>
#include <memory>

#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ClientDBLogic.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/CryptManager.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/Party.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{

   class ClientDBService : public ServiceThread<ClientDBLogic>
   {
      Q_OBJECT

   public:
      ClientDBService(QObject* parent = nullptr);

   signals:
      ////////// PROXY SIGNALS //////////
      void Init(const Chat::LoggerPtr& loggerPtr, QString chatDbFile,
         const Chat::ChatUserPtr& chatUserPtr, const Chat::CryptManagerPtr& cryptManagerPtr);
      void saveMessage(const Chat::PartyPtr& partyPtr, const std::string& data);
      void updateMessageState(const std::string& message_id, int party_message_state);
      void createNewParty(const Chat::PartyPtr& partyPtr);
      void readUnsentMessages(const std::string& partyId);
      void updateDisplayNameForParty(const std::string& partyId, const std::string& displayName);
      void loadPartyDisplayName(const std::string& partyId);
      void checkUnsentMessages(const std::string& partyId);
      void readHistoryMessages(const std::string& partyId, const std::string& userHash, int limit = std::numeric_limits<int>::max(), int offset = 0);
      void saveRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void deleteRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void updateRecipientKeys(const Chat::PartyRecipientsPtrList& recipients);
      void checkRecipientPublicKey(const Chat::UniqieRecipientMap& uniqueRecipientMap);
      void cleanUnusedParties();
      void savePartyRecipients(const Chat::PartyPtr& partyPtr);

      ////////// RETURN SIGNALS //////////
      void initDone();
      void messageArrived(const Chat::MessagePtrList& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, int party_message_state);
      void messageLoaded(const std::string& partyId, const std::string& messageId, qint64 timestamp,
         const std::string& message, int encryptionType, const std::string& nonce, int party_message_state);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void unsentMessagesFound(const std::string& partyId);
      void recipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList);
      void recipientKeysUnchanged();
   };

   using ClientDBServicePtr = std::shared_ptr<ClientDBService>;
}

Q_DECLARE_METATYPE(Chat::PartyRecipientsPtrList)
Q_DECLARE_METATYPE(Chat::UniqieRecipientMap)
Q_DECLARE_METATYPE(Chat::PartyPtr)

#endif // CLIENTDBSERVICE_H
