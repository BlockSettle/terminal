#ifndef ClientPartyLogic_h__
#define ClientPartyLogic_h__

#include <QObject>
#include <memory>
#include <google/protobuf/message.h>

#include "ChatProtocol/ClientPartyModel.h"
#include "ChatProtocol/ClientDBService.h"
#include "ChatProtocol/ChatUser.h"

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
      DynamicPointerCast
   };

   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class ClientPartyLogic : public QObject
   {
      Q_OBJECT
   public:
      ClientPartyLogic(const LoggerPtr& loggerPtr, const ClientDBServicePtr& clientDBServicePtr, QObject* parent = nullptr);

      Chat::ClientPartyModelPtr clientPartyModelPtr() const { return clientPartyModelPtr_; }
      void setClientPartyModelPtr(Chat::ClientPartyModelPtr val) { clientPartyModelPtr_ = val; }

      void handlePartiesFromWelcomePacket(const google::protobuf::Message& msg);

      void createPrivateParty(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName);

   signals:
      void error(const Chat::ClientPartyLogicError& errorCode, const std::string& what);
      void partyModelChanged();
      void sendPartyMessagePacket(const google::protobuf::Message& message);
      void privatePartyCreated(const PartyPtr& partyPtr);

   public slots:
      void onUserStatusChanged(const std::string& userName, const ClientStatus& clientStatus);

   private slots:
      void handleLocalErrors(const Chat::ClientPartyLogicError& errorCode, const std::string& what);
      void handlePartyInserted(const Chat::PartyPtr& partyPtr);

   private:
      LoggerPtr loggerPtr_;
      ClientPartyModelPtr clientPartyModelPtr_;
      ClientDBServicePtr clientDBServicePtr_;
   };

   using ClientPartyLogicPtr = std::shared_ptr<ClientPartyLogic>;
}

Q_DECLARE_METATYPE(Chat::ClientPartyLogicError)

#endif // ClientPartyLogic_h__
