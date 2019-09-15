#ifndef CLIENTDBSERVICE_H
#define CLIENTDBSERVICE_H

#include <QObject>
#include <memory>

#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ClientDBLogic.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/CryptManager.h"
#include "ChatProtocol/Message.h"

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
      void Init(const Chat::LoggerPtr& loggerPtr, const Chat::ApplicationSettingsPtr& appSettings, 
         const Chat::ChatUserPtr& chatUserPtr, const Chat::CryptManagerPtr& cryptManagerPtr);
      void saveMessage(const std::string& data);
      void updateMessageState(const std::string& message_id, const int party_message_state);
      void createNewParty(const std::string& partyId);
      void readUnsentMessages(const std::string& partyId);
      void updateDisplayNameForParty(const std::string& partyId, const std::string& displayName);
      void loadPartyDisplayName(const std::string& partyId);
      void checkUnsentMessages(const std::string& partyId);
      void readHistoryMessages(const std::string& partyId, const int limit = std::numeric_limits<int>::max(), const int offset = 0);
      void saveRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void deleteRecipientsKeys(const Chat::PartyRecipientsPtrList& recipients);
      void updateRecipientKeys(const Chat::PartyRecipientsPtrList& recipients);
      void checkRecipientPublicKey(const Chat::UniqieRecipientMap& uniqueRecipientMap);

      ////////// RETURN SIGNALS //////////
      void initDone();
      void messageArrived(const Chat::MessagePtrList& messagePtr);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
      void messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
         const std::string& message, const int encryptionType, const std::string& nonce, const int party_message_state);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void unsentMessagesFound(const std::string& partyId);
      void recipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList);
      void recipientKeysUnchanged();
   };

   using ClientDBServicePtr = std::shared_ptr<ClientDBService>;
}

Q_DECLARE_METATYPE(Chat::ApplicationSettingsPtr)
Q_DECLARE_METATYPE(Chat::PartyRecipientsPtrList)
Q_DECLARE_METATYPE(Chat::UniqieRecipientMap)

#endif // CLIENTDBSERVICE_H
