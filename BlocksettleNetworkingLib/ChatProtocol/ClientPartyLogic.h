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
      DynamicPointerCast,
      QObjectCast
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
      void createPrivatePartyFromPrivatePartyRequest(const ChatUserPtr& currentUserPtr, const google::protobuf::Message& msg);

   signals:
      void error(const Chat::ClientPartyLogicError& errorCode, const std::string& what = "");
      void partyModelChanged();
      void sendPartyMessagePacket(const google::protobuf::Message& message);
      void privatePartyCreated(const std::string& partyId);
      void privatePartyAlreadyExist(const std::string& partyId);

   public slots:
      void onUserStatusChanged(const std::string& userName, const ClientStatus& clientStatus);
      void partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName);
      void loggedOutFromServer();

   private slots:
      void handleLocalErrors(const Chat::ClientPartyLogicError& errorCode, const std::string& what);
      void handlePartyInserted(const Chat::PartyPtr& partyPtr);
      void clientPartyDisplayNameChanged();

   private:
      LoggerPtr loggerPtr_;
      ClientPartyModelPtr clientPartyModelPtr_;
      ClientDBServicePtr clientDBServicePtr_;
   };

   using ClientPartyLogicPtr = std::shared_ptr<ClientPartyLogic>;
}

Q_DECLARE_METATYPE(Chat::ClientPartyLogicError)

#endif // ClientPartyLogic_h__
