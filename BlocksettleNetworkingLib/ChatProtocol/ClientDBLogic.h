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

class QSqlDatabase;
class ApplicationSettings;

namespace Chat
{
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;

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
      ReadHistoryMessages,
      CannotOpenDatabase,
      InsertRecipientKey,
      DeleteRecipientKey,
      UpdateRecipientKey,
      CheckRecipientKey
   };

   class ClientDBLogic : public DatabaseExecutor
   {
      Q_OBJECT

   public:
      ClientDBLogic(QObject* parent = nullptr);

   public slots:
      void Init(const Chat::LoggerPtr& loggerPtr, const Chat::ApplicationSettingsPtr& appSettings, const Chat::ChatUserPtr& chatUserPtr,
         const Chat::CryptManagerPtr& cryptManagerPtr);
      void updateMessageState(const std::string& message_id, const int party_message_state);
      void saveMessage(const std::string& data);
      void createNewParty(const std::string& partyId);
      void readUnsentMessages(const std::string& partyId);
      void deleteMessage(const std::string& messageId);
      void updateDisplayNameForParty(const std::string& partyId, const std::string& displayName);
      void loadPartyDisplayName(const std::string& partyId);
      void checkUnsentMessages(const std::string& partyId);
      void readHistoryMessages(const std::string& partyId, const int limit = std::numeric_limits<int>::max(), const int offset = 0);
      void saveRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void deleteRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void updateRecipientKeys(const Chat::PartyRecipientsPtrList& recipients);
      void checkRecipientPublicKey(const Chat::UniqieRecipientMap& uniqueRecipientMap);

   signals:
      void initDone();
      void error(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");
      void messageArrived(const Chat::MessagePtrList& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
      void messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
         const std::string& message, const int encryptionType, const std::string& nonce, const int party_message_state);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void unsentMessagesFound(const std::string& partyId);
      void recipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList);
      void recipientKeysUnchanged();

   private slots:
      void rebuildError();
      void handleLocalErrors(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");

   private:
      bool getPartyIdByMessageId(const std::string& messageId, std::string& partyId);
      bool getPartyIdFromDB(const std::string& partyId, std::string& partyTableId);
      bool insertPartyId(const std::string& partyId, std::string& partyTableId);
      QSqlDatabase getDb();

      ApplicationSettingsPtr     applicationSettingsPtr_;
      ClientDatabaseCreatorPtr   databaseCreatorPtr_;
      CryptManagerPtr            cryptManagerPtr_;
      ChatUserPtr                currentChatUserPtr_;
   };

   using ClientDBLogicPtr = std::shared_ptr<ClientDBLogic>;

}

Q_DECLARE_METATYPE(Chat::ClientDBLogicError)

#endif // CLIENTDBLOGIC_H
