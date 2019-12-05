/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CLIENTDBLOGIC_H
#define CLIENTDBLOGIC_H

#include <memory>

#include "ChatProtocol/DatabaseExecutor.h"
#include "ChatProtocol/ClientDatabaseCreator.h"
#include "ChatProtocol/CryptManager.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/PartyRecipient.h"
#include "ChatProtocol/SessionKeyHolder.h"
#include "ChatProtocol/UserPublicKeyInfo.h"
#include "ChatProtocol/Party.h"

class QSqlDatabase;

namespace Chat
{
   enum class ClientDBLogicError
   {
      InitDatabase,
      InsertPartyId,
      GetTablePartyId,
      SaveMessage,
      UpdateMessageState,
      PartyMessagePacketCasting,
      DeleteMessage,
      UpdatePartyDisplayName,
      CheckUnsentMessages,
      ReadPrivateHistoryMessages,
      CannotOpenDatabase,
      InsertRecipientKey,
      DeleteRecipientKey,
      CheckRecipientKey,
      CleanUnusedParties,
      CleanUnusedPartyToUser,
      InsertPartyToUser,
      InsertUser
   };

   class ClientDBLogic : public DatabaseExecutor
   {
      Q_OBJECT

   public:
      ClientDBLogic(QObject* parent = nullptr);

   public slots:
      void Init(const Chat::LoggerPtr& loggerPtr, QString chatDbFile, const Chat::ChatUserPtr& chatUserPtr,
         const Chat::CryptManagerPtr& cryptManagerPtr);
      void updateMessageState(const std::string& message_id, int party_message_state);
      void saveMessage(const Chat::PartyPtr& partyPtr, const std::string& data);
      void createNewParty(const Chat::PartyPtr& partyPtr);
      void readUnsentMessages(const std::string& partyId);
      void deleteMessage(const std::string& messageId);
      void updateDisplayNameForParty(const std::string& partyId, const std::string& displayName);
      void loadPartyDisplayName(const std::string& partyId);
      void checkUnsentMessages(const std::string& partyId);
      void readPrivateHistoryMessages(const std::string& partyId, const std::string& userHash, int limit = std::numeric_limits<int>::max(), int offset = 0);
      void saveRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void deleteRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void updateRecipientKeys(const Chat::PartyRecipientsPtrList& recipients);
      void checkRecipientPublicKey(const Chat::UniqieRecipientMap& uniqueRecipientMap);
      void clearUnusedParties();
      void savePartyRecipients(const Chat::PartyPtr& partyPtr);

   signals:
      void initDone();
      void error(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");
      void messageArrived(const Chat::MessagePtrList& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, int party_message_state);
      void messageLoaded(const std::string& partyId, const std::string& messageId, qint64 timestamp,
         const std::string& message, int encryptionType, const std::string& nonce, int party_message_state);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void unsentMessagesFound(const std::string& partyId);
      void recipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList);
      void recipientKeysUnchanged();

   private slots:
      void rebuildError();
      void handleLocalErrors(const Chat::ClientDBLogicError& errorCode, const std::string& what = "") const;

   private:
      bool getPartyIdByMessageId(const std::string& messageId, std::string& partyId);
      bool getPartyTableIdFromDB(const PartyPtr& partyPtr, std::string& partyTableId);
      bool insertPartyId(const PartyPtr& partyPtr, std::string& partyTableId);
      bool getUserTableId(const std::string& userHash, std::string& userTableId);
      void saveRecipientKey(const PartyRecipientPtr& recipient);
      void insertNewUserHash(const std::string& userHash);
      QSqlDatabase getDb();

      ClientDatabaseCreatorPtr   databaseCreatorPtr_;
      CryptManagerPtr            cryptManagerPtr_;
      ChatUserPtr                currentChatUserPtr_;
      QString                    chatDbFile_;
   };

   using ClientDBLogicPtr = std::shared_ptr<ClientDBLogic>;

}

Q_DECLARE_METATYPE(Chat::ClientDBLogicError)

#endif // CLIENTDBLOGIC_H
