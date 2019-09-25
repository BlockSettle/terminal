#ifndef OTC_CLIENT_H
#define OTC_CLIENT_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <QObject>

#include "BSErrorCode.h"
#include "BinaryData.h"
#include "OtcTypes.h"

namespace spdlog {
   class logger;
}

namespace Blocksettle {
   namespace Communication {
      namespace Otc {
         class ContactMessage;
         class ContactMessage_BuyerOffers;
         class ContactMessage_SellerOffers;
         class ContactMessage_BuyerAccepts;
         class ContactMessage_SellerAccepts;
         class ContactMessage_BuyerAcks;
         class ContactMessage_Close;
         class ContactMessage_QuoteResponse;
         class PublicMessage_Request;
         class PublicMessage_Close;
         class PublicMessage_PrivateMessage;
      }
   }
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
         class Response_StartOtc;
         class Response_VerifyOtc;
      }
   }
}

namespace bs {
   class Address;
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
   namespace sync {
      class Wallet;
      class WalletsManager;
      namespace hd {
         class SettlementLeaf;
      }
   }
}

class ArmoryConnection;
class AuthAddressManager;
class SignContainer;
struct OtcClientDeal;

struct OtcClientParams
{
   // Return path that will be used to save offline sign request.
   // Must be set if offline wallet will be used for sell.
   std::function<std::string(const std::string &walletId)> offlineSavePathCb;

   // Return path that will be used to load signed offline request.
   // Must be set if offline wallet will be used for sell.
   std::function<std::string()> offlineLoadPathCb;

   bs::network::otc::Env env{};
};

class OtcClient : public QObject
{
   Q_OBJECT

public:
   // authAddressManager could be null. If not set peer's auth address won't be verified (and verification affects only signer UI for now).
   OtcClient(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<ArmoryConnection> &armory
      , const std::shared_ptr<SignContainer> &signContainer
      , const std::shared_ptr<AuthAddressManager> &authAddressManager
      , OtcClientParams params
      , QObject *parent = nullptr);
   ~OtcClient() override;

   bs::network::otc::Peer *contact(const std::string &contactId);
   bs::network::otc::Peer *request(const std::string &contactId);
   bs::network::otc::Peer *response(const std::string &contactId);

   void setOwnContactId(const std::string &contactId);
   const std::string &ownContactId() const;

   bool sendQuoteRequest(const bs::network::otc::QuoteRequest &request);
   bool sendQuoteResponse(bs::network::otc::Peer *peer, const bs::network::otc::QuoteResponse &quoteResponse);
   bool sendOffer(bs::network::otc::Peer *peer, const bs::network::otc::Offer &offer);
   bool acceptOffer(bs::network::otc::Peer *peer, const bs::network::otc::Offer &offer);
   bool updateOffer(bs::network::otc::Peer *peer, const bs::network::otc::Offer &offer);
   bool pullOrReject(bs::network::otc::Peer *peer);

   const bs::network::otc::Peers &requests() { return requests_; }
   const bs::network::otc::Peers &responses() { return responses_; }
   bs::network::otc::Peer *ownRequest() const;

public slots:
   void contactConnected(const std::string &contactId);
   void contactDisconnected(const std::string &contactId);
   void processContactMessage(const std::string &contactId, const BinaryData &data);
   void processPbMessage(const std::string &data);
   void processPublicMessage(QDateTime timestamp, const std::string &contactId, const BinaryData &data);
   void processPrivateMessage(QDateTime timestamp, const std::string &contactId, bool isResponse, const BinaryData &data);

signals:
   void sendContactMessage(const std::string &contactId, const BinaryData &data);
   void sendPbMessage(const std::string &data);
   void sendPublicMessage(const BinaryData &data);

   void peerUpdated(const bs::network::otc::Peer *peer);
   void publicUpdated();

private slots:
   void onTxSigned(unsigned reqId, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason);

private:
   using OtcClientDealCb = std::function<void(OtcClientDeal &&deal)>;

   struct SettlementIdRequest
   {
      bs::network::otc::Peer *peer{};
      ValidityHandle handle;
   };

   void processPeerMessage(bs::network::otc::Peer *peer, const BinaryData &data);

   void processQuoteResponse(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_QuoteResponse &msg);
   void processBuyerOffers(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_BuyerOffers &msg);
   void processSellerOffers(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_SellerOffers &msg);
   void processBuyerAccepts(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_BuyerAccepts &msg);
   void processSellerAccepts(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_SellerAccepts &msg);
   void processBuyerAcks(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_BuyerAcks &msg);
   void processClose(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage_Close &msg);

   void processPublicRequest(QDateTime timestamp, const std::string &contactId, const Blocksettle::Communication::Otc::PublicMessage_Request &msg);
   void processPublicClose(QDateTime timestamp, const std::string &contactId, const Blocksettle::Communication::Otc::PublicMessage_Close &msg);
   void processPublicPrivateMessage(QDateTime timestamp, const std::string &contactId, const Blocksettle::Communication::Otc::PublicMessage_PrivateMessage &msg);

   void processPbStartOtc(const Blocksettle::Communication::ProxyTerminalPb::Response_StartOtc &response);
   void processPbVerifyOtc(const Blocksettle::Communication::ProxyTerminalPb::Response_VerifyOtc &response);

   // Checks that hdWallet, auth address and recv address (is set) are valid
   bool verifyOffer(const bs::network::otc::Offer &offer) const;
   void blockPeer(const std::string &reason, bs::network::otc::Peer *peer);

   void send(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::ContactMessage &msg);

   void createRequests(const BinaryData &settlementId, bs::network::otc::Peer *peer, const OtcClientDealCb &cb);
   void sendSellerAccepts(bs::network::otc::Peer *peer);

   std::shared_ptr<bs::sync::hd::SettlementLeaf> findSettlementLeaf(const std::string &ourAuthAddress);

   void changePeerState(bs::network::otc::Peer *peer, bs::network::otc::State state);
   int genLocalUniqueId() { return ++latestUniqueId_; }
   void trySendSignedTxs(OtcClientDeal *deal);
   void verifyAuthAddresses(OtcClientDeal *deal);
   void setComments(OtcClientDeal *deal);

   void updatePublicLists();

   std::shared_ptr<spdlog::logger> logger_;

   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<ArmoryConnection> armory_;
   std::shared_ptr<SignContainer> signContainer_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;

   std::string ownContactId_;

   std::map<BinaryData, std::unique_ptr<OtcClientDeal>> deals_;

   int latestUniqueId_{};
   std::map<int, SettlementIdRequest> waitSettlementIds_;

   // Maps sign requests to settlementId
   std::map<unsigned, BinaryData> signRequestIds_;

   std::unique_ptr<bs::network::otc::Peer> ownRequest_;
   std::unordered_map<std::string, bs::network::otc::Peer> contactMap_;
   std::unordered_map<std::string, bs::network::otc::Peer> requestMap_;
   std::unordered_map<std::string, bs::network::otc::Peer> responseMap_;

   bs::network::otc::Peers contacts_;
   bs::network::otc::Peers requests_;
   bs::network::otc::Peers responses_;

   OtcClientParams params_;
};

#endif
