#ifndef CLIENTPARTY_H
#define CLIENTPARTY_H

#include <QObject>
#include <memory>
#include <vector>

#include "ChatProtocol/PrivateDirectMessageParty.h"

#include "chat.pb.h"

namespace Chat {

   extern const char *GlobalRoomName;
   extern const char *OtcRoomName;
   extern const char *SupportRoomName;

   class ClientParty : public QObject, public PrivateDirectMessageParty
   {
      Q_OBJECT
   public:
      ClientParty(
         const std::string& id, 
         const PartyType& partyType, 
         const PartySubType& partySubType = STANDARD, 
         const PartyState& partyState = UNINITIALIZED, 
         QObject* parent = nullptr
      );

      std::string displayName() const { return displayName_; }
      void setDisplayName(const std::string& val);

      ClientStatus clientStatus() const { return clientStatus_; }
      void setClientStatus(const ClientStatus& val);

      std::string userHash() const { return userHash_; }
      void setUserHash(const std::string& val) { userHash_ = val; }

      void setPartyState(const PartyState& val) override;

      std::string initialMessage() const { return initialMessage_; }
      void setInitialMessage(const std::string& val) { initialMessage_ = val; }

   signals:
      void clientStatusChanged(const ClientStatus& clientStatus);
      void displayNameChanged();
      void partyStateChanged(const std::string& partyId);

   private:
      std::string displayName_;
      std::string userHash_;
      ClientStatus clientStatus_;
      std::string initialMessage_;
   };

   using ClientPartyPtr = std::shared_ptr<ClientParty>;
   using ClientPartyPtrList = std::vector<ClientPartyPtr>;

} // namespace Chat

Q_DECLARE_METATYPE(Chat::ClientPartyPtr)

#endif // CLIENTPARTY_H
