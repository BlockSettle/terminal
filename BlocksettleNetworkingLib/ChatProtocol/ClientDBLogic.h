#ifndef ClientDBLogic_h__
#define ClientDBLogic_h__

#include <memory>

#include "ChatProtocol/DatabaseExecutor.h"
#include "ChatProtocol/ClientDatabaseCreator.h"
#include "ChatProtocol/CryptManager.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/Message.h"

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
      UpdatePartyDisplayName
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

   signals:
      void initDone();
      void error(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");
      void messageArrived(const Chat::MessagePtr& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
      void messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
         const std::string& message, const int encryptionType, const std::string& nonce, const int party_message_state);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);

   private slots:
      void rebuildError();
      void handleLocalErrors(const Chat::ClientDBLogicError& errorCode, const std::string& what = "");

   private:
      bool getPartyIdByMessageId(const std::string& messageId, std::string& partyId);
      bool getPartyIdFromDB(const std::string& partyId, std::string& partyTableId);
      bool insertPartyId(const std::string& partyId, std::string& partyTableId);
      QSqlDatabase getDb() const;

      ApplicationSettingsPtr     applicationSettingsPtr_;
      ClientDatabaseCreatorPtr   databaseCreatorPtr_;
      CryptManagerPtr            cryptManagerPtr_;
      ChatUserPtr                currentChatUserPtr_;
   };

   using ClientDBLogicPtr = std::shared_ptr<ClientDBLogic>;

}

Q_DECLARE_METATYPE(Chat::ClientDBLogicError);

#endif // ClientDBLogic_h__