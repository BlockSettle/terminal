#ifndef CHATOTCHELPER_H
#define CHATOTCHELPER_H

#include <QObject>
#include <set>
#include "OtcTypes.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientParty.h"

namespace spdlog {
   class logger;
}

namespace bs {
   namespace sync {
      class WalletsManager;
   }
   namespace network {
      namespace otc {
         enum class Env : int;
         struct Offer;
         struct Peer;
         struct PeerId;
         struct QuoteRequest;
         struct QuoteResponse;
      }
   }
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
}

class ApplicationSettings;
class ArmoryConnection;
class AuthAddressManager;
class OtcClient;
class SignContainer;

class ChatOTCHelper : public QObject {
   Q_OBJECT
public:
   ChatOTCHelper(QObject* parent = nullptr);
   ~ChatOTCHelper() override = default;

   void init(bs::network::otc::Env env
      , const std::shared_ptr<spdlog::logger>& loggerPtr
      , const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
      , const std::shared_ptr<ArmoryConnection>& armory
      , const std::shared_ptr<SignContainer>& signContainer
      , const std::shared_ptr<AuthAddressManager> &authAddressManager
      , const std::shared_ptr<ApplicationSettings> &applicationSettings);

   OtcClient* client() const;

   void setCurrentUserId(const std::string& ownUserId);

   void setGlobalOTCEntryTimeStamp(QDateTime timeStamp);
   QDateTime selectedGlobalOTCEntryTimeStamp() const;

public slots:
   void onLogout();
   void onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response);

   void onOtcRequestSubmit(bs::network::otc::Peer *peer, const bs::network::otc::Offer& offer);
   void onOtcPullOrReject(bs::network::otc::Peer *peer);
   void onOtcResponseAccept(bs::network::otc::Peer *peer, const bs::network::otc::Offer& offer);
   void onOtcResponseUpdate(bs::network::otc::Peer *peer, const bs::network::otc::Offer& offer);
   void onOtcResponseReject(bs::network::otc::Peer *peer);

   void onOtcQuoteRequestSubmit(const bs::network::otc::QuoteRequest &request);
   void onOtcQuoteResponseSubmit(bs::network::otc::Peer *peer, const bs::network::otc::QuoteResponse &response);

   void onMessageArrived(const Chat::MessagePtrList& messagePtr);
   void onPartyStateChanged(const Chat::ClientPartyPtr& clientPartyPtr);

private:
   OtcClient* otcClient_{};
   std::set<std::string> connectedContacts_;
   std::shared_ptr<spdlog::logger> loggerPtr_;
   QDateTime selectedGlobalEntryTimeStamp_{};
};

#endif // CHATOTCHELPER_H
