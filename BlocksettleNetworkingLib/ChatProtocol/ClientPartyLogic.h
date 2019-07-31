#ifndef ClientPartyLogic_h__
#define ClientPartyLogic_h__

#include <QObject>
#include <memory>
#include <google/protobuf/message.h>

#include "ChatProtocol/ClientPartyModel.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   enum class ClientPartyLogicError
   {
      NonexistentClientStatusChanged
   };

   Q_DECLARE_METATYPE(ClientPartyLogicError)

   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class ClientPartyLogic : public QObject
   {
      Q_OBJECT
   public:
      ClientPartyLogic(const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      Chat::ClientPartyModelPtr clientPartyModelPtr() const { return clientPartyModelPtr_; }
      void setClientPartyModelPtr(Chat::ClientPartyModelPtr val) { clientPartyModelPtr_ = val; }

      void handlePartiesFromWelcomePacket(const google::protobuf::Message& msg);

   signals:
      void error(const ClientPartyLogicError& errorCode, const std::string& what);
      void partyModelChanged();

   public slots:
      void onUserStatusChanged(const std::string& userName, const ClientStatus& clientStatus);

   private slots:
      void handleLocalErrors(const ClientPartyLogicError& errorCode, const std::string& what);

   private:
      LoggerPtr loggerPtr_;
      ClientPartyModelPtr clientPartyModelPtr_;
   };

   using ClientPartyLogicPtr = std::shared_ptr<ClientPartyLogic>;
}

#endif // ClientPartyLogic_h__
