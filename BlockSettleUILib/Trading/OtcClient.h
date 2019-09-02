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
         class Message;
         class Message_BuyerOffers;
         class Message_SellerOffers;
         class Message_BuyerAccepts;
         class Message_SellerAccepts;
         class Message_BuyerAcks;
         class Message_Close;
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
class SignContainer;
struct OtcClientDeal;

class OtcClient : public QObject
{
   Q_OBJECT
public:
   OtcClient(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<ArmoryConnection> &armory
      , const std::shared_ptr<SignContainer> &signContainer
      , QObject *parent = nullptr);
   ~OtcClient() override;

   const bs::network::otc::Peer *peer(const std::string &peerId) const;

   void setCurrentUserId(const std::string &userId);

   bool sendOffer(const bs::network::otc::Offer &offer, const std::string &peerId);
   bool pullOrRejectOffer(const std::string &peerId);
   bool acceptOffer(const bs::network::otc::Offer &offer, const std::string &peerId);
   bool updateOffer(const bs::network::otc::Offer &offer, const std::string &peerId);

public slots:
   void peerConnected(const std::string &peerId);
   void peerDisconnected(const std::string &peerId);
   void processMessage(const std::string &peerId, const BinaryData &data);
   void processPbMessage(const std::string &data);

signals:
   void sendMessage(const std::string &peerId, const BinaryData &data);
   void sendPbMessage(const std::string &data);

   void peerUpdated(const std::string &peerId);

private slots:
   void onTxSigned(unsigned reqId, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason);

private:
   using OtcClientDealCb = std::function<void(OtcClientDeal &&deal)>;

   void processBuyerOffers(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_BuyerOffers &msg);
   void processSellerOffers(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_SellerOffers &msg);
   void processBuyerAccepts(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_BuyerAccepts &msg);
   void processSellerAccepts(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_SellerAccepts &msg);
   void processBuyerAcks(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_BuyerAcks &msg);
   void processClose(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_Close &msg);

   void processPbStartOtc(const Blocksettle::Communication::ProxyTerminalPb::Response_StartOtc &response);
   void processPbVerifyOtc(const Blocksettle::Communication::ProxyTerminalPb::Response_VerifyOtc &response);

   void blockPeer(const std::string &reason, bs::network::otc::Peer *peer);

   bs::network::otc::Peer *findPeer(const std::string &peerId);

   void send(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message &msg);

   void createRequests(const BinaryData &settlementId, const bs::network::otc::Peer &peer, const OtcClientDealCb &cb);

   void sendSellerAccepts(bs::network::otc::Peer *peer);

   std::shared_ptr<bs::sync::hd::SettlementLeaf> ourSettlementLeaf();
   std::shared_ptr<bs::sync::Wallet> ourBtcWallet();

   void changePeerState(bs::network::otc::Peer *peer, bs::network::otc::State state);

   int genLocalUniqueId() { return ++latestUniqueId_; }

   void trySendSignedTxs(OtcClientDeal *deal);

   std::shared_ptr<spdlog::logger> logger_;
   std::unordered_map<std::string, bs::network::otc::Peer> peers_;

   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;

   std::shared_ptr<ArmoryConnection> armory_;
   std::shared_ptr<SignContainer> signContainer_;

   BinaryData ourPubKey_;

   std::string currentUserId_;

   std::map<BinaryData, std::unique_ptr<OtcClientDeal>> deals_;

   int latestUniqueId_{};
   std::map<int, bs::network::otc::Peer> waitSettlementIds_;

   // Maps sign requests to settlementId
   std::map<unsigned, BinaryData> signRequestIds_;
};

#endif
