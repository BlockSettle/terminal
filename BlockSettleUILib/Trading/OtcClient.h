/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef OTC_CLIENT_H
#define OTC_CLIENT_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <QObject>

#include "BSErrorCode.h"
#include "BinaryData.h"
#include "OtcTypes.h"
#include "UtxoReservationToken.h"

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
         class Response_UpdateOtcState;
      }
   }
}

namespace bs {
   namespace tradeutils {
      struct Args;
   }
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
   class UTXOReservationManager;
}

class ApplicationSettings;
class ArmoryConnection;
class AuthAddressManager;
class WalletSignerContainer;
struct OtcClientDeal;

struct OtcClientParams
{
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
      , const std::shared_ptr<WalletSignerContainer> &signContainer
      , const std::shared_ptr<AuthAddressManager> &authAddressManager
      , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
      , const std::shared_ptr<ApplicationSettings>& applicationSettings
      , OtcClientParams params
      , QObject *parent = nullptr);
   ~OtcClient() override;

   bs::network::otc::PeerPtr contact(const std::string &contactId);
   bs::network::otc::PeerPtr request(const std::string &contactId);
   bs::network::otc::PeerPtr response(const std::string &contactId);

   // Calls one of above methods depending on type
   bs::network::otc::PeerPtr peer(const std::string &contactId, bs::network::otc::PeerType type);

   void setOwnContactId(const std::string &contactId);
   const std::string &ownContactId() const;

   bool sendQuoteRequest(const bs::network::otc::QuoteRequest &request);
   bool sendQuoteResponse(const bs::network::otc::PeerPtr &peer, const bs::network::otc::QuoteResponse &quoteResponse);
   bool sendOffer(const bs::network::otc::PeerPtr &peer, const bs::network::otc::Offer &offer);
   bool acceptOffer(const bs::network::otc::PeerPtr &peer, const bs::network::otc::Offer &offer);
   bool updateOffer(const bs::network::otc::PeerPtr &peer, const bs::network::otc::Offer &offer);
   bool pullOrReject(const bs::network::otc::PeerPtr &peer);
   void setReservation(const bs::network::otc::PeerPtr &peer, bs::UtxoReservationToken&& reserv);
   bs::UtxoReservationToken releaseReservation(const bs::network::otc::PeerPtr &peer);

   const bs::network::otc::PeerPtrs &requests() { return requests_; }
   const bs::network::otc::PeerPtrs &responses() { return responses_; }
   const bs::network::otc::PeerPtr &ownRequest() const;

public slots:
   void contactConnected(const std::string &contactId);
   void contactDisconnected(const std::string &contactId);
   void processContactMessage(const std::string &contactId, const BinaryData &data);
   void processPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void processPublicMessage(QDateTime timestamp, const std::string &contactId, const BinaryData &data);

signals:
   void sendContactMessage(const std::string &contactId, const BinaryData &data);
   void sendPbMessage(const std::string &data);
   void sendPublicMessage(const BinaryData &data);

   void peerUpdated(const bs::network::otc::PeerPtr &peer);
   // Used to update UI when there is some problems (for example deal verification failed)
   void peerError(const bs::network::otc::PeerPtr &peer, bs::network::otc::PeerErrorType type, const std::string *errorMsg);

   void publicUpdated();

private slots:
   void onTxSigned(unsigned reqId, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason);

private:
   using OtcClientDealCb = std::function<void(OtcClientDeal &&deal)>;

   struct SettlementIdRequest
   {
      bs::network::otc::PeerPtr peer;
      ValidityHandle handle;
   };

   void processQuoteResponse(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_QuoteResponse &msg);
   void processBuyerOffers(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_BuyerOffers &msg);
   void processSellerOffers(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_SellerOffers &msg);
   void processBuyerAccepts(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_BuyerAccepts &msg);
   void processSellerAccepts(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_SellerAccepts &msg);
   void processBuyerAcks(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_BuyerAcks &msg);
   void processClose(const bs::network::otc::PeerPtr &peer, const Blocksettle::Communication::Otc::ContactMessage_Close &msg);

   void processPublicRequest(QDateTime timestamp, const std::string &contactId, const Blocksettle::Communication::Otc::PublicMessage_Request &msg);
   void processPublicClose(QDateTime timestamp, const std::string &contactId, const Blocksettle::Communication::Otc::PublicMessage_Close &msg);

   void processPbStartOtc(const Blocksettle::Communication::ProxyTerminalPb::Response_StartOtc &response);
   void processPbUpdateOtcState(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOtcState &response);

   // Checks that hdWallet, auth address and recv address (is set) are valid
   bool verifyOffer(const bs::network::otc::Offer &offer) const;
   void blockPeer(const std::string &reason, const bs::network::otc::PeerPtr &peer);

   void send(const bs::network::otc::PeerPtr &peer, Blocksettle::Communication::Otc::ContactMessage &msg);

   void createSellerRequest(const std::string &settlementId, const bs::network::otc::PeerPtr &peer, const OtcClientDealCb &cb);
   void createBuyerRequest(const std::string &settlementId, const bs::network::otc::PeerPtr &peer, const OtcClientDealCb &cb);
   void sendSellerAccepts(const bs::network::otc::PeerPtr &peer);

   std::shared_ptr<bs::sync::hd::SettlementLeaf> findSettlementLeaf(const std::string &ourAuthAddress);

   void changePeerStateWithoutUpdate(const bs::network::otc::PeerPtr &peer, bs::network::otc::State state);
   void changePeerState(const bs::network::otc::PeerPtr &peer, bs::network::otc::State state);
   void resetPeerStateToIdle(const bs::network::otc::PeerPtr &peer);
   void scheduleCloseAfterTimeout(std::chrono::milliseconds timeout, const bs::network::otc::PeerPtr &peer);

   int genLocalUniqueId() { return ++latestUniqueId_; }
   void trySendSignedTx(OtcClientDeal *deal);
   void verifyAuthAddresses(OtcClientDeal *deal);
   void setComments(OtcClientDeal *deal);

   void updatePublicLists();

   void initTradesArgs(bs::tradeutils::Args &args, const bs::network::otc::PeerPtr &peer, const std::string &settlementId);

   bool expandTxDialog() const;
   bool authAddressVerificationRequired(OtcClientDeal *deal) const;

   std::shared_ptr<spdlog::logger> logger_;

   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<WalletSignerContainer> signContainer_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;
   std::shared_ptr<ApplicationSettings> applicationSettings_;

   std::string ownContactId_;

   // Maps settlementId to OtcClientDeal
   std::map<std::string, std::unique_ptr<OtcClientDeal>> deals_;

   int latestUniqueId_{};
   std::map<int, SettlementIdRequest> waitSettlementIds_;

   // Maps sign requests to settlementId
   std::map<unsigned, std::string> signRequestIds_;

   // Own public request if exists
   bs::network::otc::PeerPtr ownRequest_;

   // Maps contactId to corresponding Peer
   std::unordered_map<std::string, bs::network::otc::PeerPtr> contactMap_;
   std::unordered_map<std::string, bs::network::otc::PeerPtr> requestMap_;
   std::unordered_map<std::string, bs::network::otc::PeerPtr> responseMap_;

   // Cached pointer lists from the above
   bs::network::otc::PeerPtrs contacts_;
   bs::network::otc::PeerPtrs requests_;
   bs::network::otc::PeerPtrs responses_;

   OtcClientParams params_;

   // Utxo reservation
   std::unordered_map<std::string, bs::UtxoReservationToken> reservedTokens_;
};

#endif
