#ifndef ConnectionLogic_h__
#define ConnectionLogic_h__

#include <memory>
#include <QObject>
#include <google/protobuf/message.h>

#include "DataConnectionListener.h"
#include "ApplicationSettings.h"
#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientDBService.h"

#include "chat.pb.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;

   enum class ClientConnectionLogicError
   {
      SendingDataToUnhandledParty,
      UnhandledPacket,
      MessageSeenForWrongTypeOfParty
   };

   class ClientConnectionLogic : public QObject
   {
      Q_OBJECT
   public:
      explicit ClientConnectionLogic(const ClientPartyLogicPtr& clientPartyLogicPtr, const ApplicationSettingsPtr& appSettings, 
         const ClientDBServicePtr& clientDBServicePtr, const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      Chat::ChatUserPtr currentUserPtr() const { return currentUserPtr_; }
      void setCurrentUserPtr(Chat::ChatUserPtr val) { currentUserPtr_ = val; }
      void SendPartyMessage(const std::string& partyId, const std::string& data);

      void prepareAndSendMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void prepareAndSendGlobalMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data);
      void setMessageSeen(const ClientPartyPtr& clientPartyPtr, const std::string& messageId);

   public slots:
      void onDataReceived(const std::string&);
      void onConnected(void);
      void onDisconnected(void);
      void onError(DataConnectionListener::DataConnectionError);

      void messagePacketSent(const std::string& messageId);

   signals:
      void sendPacket(const google::protobuf::Message& message);
      void closeConnection();
      void userStatusChanged(const std::string& userName, const ClientStatus& clientStatus);
      void error(const Chat::ClientConnectionLogicError& errorCode, const std::string& what);

      // TODO: remove
      void testProperlyConnected();

   private slots:
      void handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what = "");

   private:
//      template<typename T>
//      bool pbStringToMessage(const std::string& packetString, google::protobuf::Message* msg);

      void handleWelcomeResponse(const google::protobuf::Message& msg);
      void handleLogoutResponse(const google::protobuf::Message& msg);
      void handleStatusChanged(const google::protobuf::Message& msg);
      void handlePartyMessageStateUpdate(const google::protobuf::Message& msg);
      void handlePartyMessagePacket(const google::protobuf::Message& msg);

      LoggerPtr   loggerPtr_;
      ChatUserPtr currentUserPtr_;
      ApplicationSettingsPtr appSettings_;
      ClientPartyLogicPtr clientPartyLogicPtr_;
      ClientDBServicePtr clientDBServicePtr_;
   };

   using ClientConnectionLogicPtr = std::shared_ptr<ClientConnectionLogic>;
}

#endif // ConnectionLogic_h__
