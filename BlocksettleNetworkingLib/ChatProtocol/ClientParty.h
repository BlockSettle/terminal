#ifndef CLIENTPARTY_H
#define CLIENTPARTY_H

#include <QObject>
#include <memory>

#include "ChatProtocol/PrivateDirectMessageParty.h"

#include "chat.pb.h"

namespace Chat
{
   
   class ClientParty : public QObject, public PrivateDirectMessageParty
   {
      Q_OBJECT
   public:
      ClientParty(
         const std::string& id, 
         const PartyType& partyType, 
         const PartySubType& partySubType = PartySubType::STANDARD, 
         const PartyState& partyState = PartyState::UNINITIALIZED, 
         QObject* parent = nullptr
      );

      std::string displayName() const { return displayName_; }
      void setDisplayName(std::string val);

      ClientStatus clientStatus() const { return clientStatus_; }
      void setClientStatus(ClientStatus val);

      std::string userHash() const { return userHash_; }
      void setUserHash(std::string val) { userHash_ = val; }

      void setPartyState(Chat::PartyState val);

   signals:
      void clientStatusChanged(const ClientStatus& clientStatus);
      void displayNameChanged();
      void partyStateChanged(const std::string& partyId);

   private:
      std::string displayName_;
      std::string userHash_;
      ClientStatus clientStatus_;
   };

   using ClientPartyPtr = std::shared_ptr<ClientParty>;

}

Q_DECLARE_METATYPE(Chat::ClientPartyPtr)

#endif // CLIENTPARTY_H
