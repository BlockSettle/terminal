#ifndef ClientDBService_h__
#define ClientDBService_h__

#include <QObject>
#include <memory>

#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ClientDBLogic.h"
#include "ChatProtocol/ChatUser.h"

#include <google/protobuf/message.h>

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
      void Init(const Chat::LoggerPtr& loggerPtr, const Chat::ApplicationSettingsPtr& appSettings, const ChatUserPtr& chatUserPtr);
      void saveMessage(const std::string& data);
      void updateMessageState(const std::string& message_id, const int party_message_state);
      void createNewParty(const std::string& partyId);

      ////////// RETURN SIGNALS //////////
      void initDone();
      void messageInserted(const std::string& partyId, const std::string& messageId, const std::string& message,
         const qint64 timestamp, const int party_message_state);
      void messageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
   };

   using ClientDBServicePtr = std::shared_ptr<ClientDBService>;
}

Q_DECLARE_METATYPE(Chat::ApplicationSettingsPtr);

#endif // ClientDBService_h__
