#ifndef ConnectionLogic_h__
#define ConnectionLogic_h__

#include <memory>
#include <QObject>
#include <google/protobuf/message.h>

#include "DataConnectionListener.h"
#include "ApplicationSettings.h"
#include "ChatProtocol/ChatUser.h"

namespace spdlog
{
   class logger;
}

namespace google {
   namespace protobuf {
      class Message;
   }
}

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;

   class ClientConnectionLogic : public QObject
   {
      Q_OBJECT
   public:
      explicit ClientConnectionLogic(const ApplicationSettingsPtr& appSettings, const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      Chat::ChatUserPtr currentUserPtr() const { return currentUserPtr_; }
      void setCurrentUserPtr(Chat::ChatUserPtr val) { currentUserPtr_ = val; }

   public slots:
      void onDataReceived(const std::string&);
      void onConnected(void);
      void onDisconnected(void);
      void onError(DataConnectionListener::DataConnectionError);

   signals:
      void sendRequestPacket(const google::protobuf::Message& message);
      void closeConnection();

   private:
      template<typename T>
      bool pbStringToMessage(const std::string& packetString, google::protobuf::Message* msg);

      void handleWelcomeResponse(const google::protobuf::Message& msg);
      void handleLogoutResponse(const google::protobuf::Message& msg);

      LoggerPtr   loggerPtr_;
      ChatUserPtr currentUserPtr_;
      ApplicationSettingsPtr appSettings_;
   };

   using ConnectionLogicPtr = std::shared_ptr<ClientConnectionLogic>;
}

#endif // ConnectionLogic_h__
