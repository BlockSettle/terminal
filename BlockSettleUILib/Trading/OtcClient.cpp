#include "OtcClient.h"

#include <QTimer>
#include <spdlog/spdlog.h>

#include "BtcUtils.h"
#include "CommonTypes.h"
#include "EncryptionUtils.h"
#include "SettlementMonitor.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "bs_proxy_terminal_pb.pb.h"
#include "otc.pb.h"

using namespace Blocksettle::Communication::Otc;
using namespace Blocksettle::Communication;
using namespace bs::network;
using namespace bs::network::otc;

namespace {

   const int kSettlementIdSize = 32;
   const int kTxHashSize = 32;
   const int kPubKeySize = 33;

   const auto kStartOtcTimeout = std::chrono::seconds(10);

   bs::sync::PasswordDialogData toPasswordDialogData(bs::network::Side::Type side, int64_t amountSat, int64_t feeSat, int64_t priceCents)
   {
      double amount = satToBtc(amountSat);
      double fee = satToBtc(feeSat);
      double price = fromCents(priceCents);

      QString qtyProd = UiUtils::XbtCurrency;
      QString fxProd = QString::fromStdString("EUR");

      bs::sync::PasswordDialogData dialogData;

      dialogData.setValue("ProductGroup", QObject::tr(bs::network::Asset::toString(bs::network::Asset::SpotXBT)));
      dialogData.setValue("Security", QString::fromStdString("XBT/EUR"));
      dialogData.setValue("Product", QString::fromStdString("XBT"));
      dialogData.setValue("Side", QObject::tr(bs::network::Side::toString(side)));

      dialogData.setValue("Title", QObject::tr("Settlement Transaction"));

      dialogData.setValue("Price", UiUtils::displayPriceXBT(price));
      dialogData.setValue("TransactionAmount", UiUtils::displayQuantity(amount, UiUtils::XbtCurrency));

      dialogData.setValue("Quantity", QObject::tr("%1 %2")
                          .arg(UiUtils::displayAmountForProduct(amount, qtyProd, bs::network::Asset::Type::SpotXBT))
                          .arg(qtyProd));
      dialogData.setValue("TotalValue", QObject::tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(amount * price, fxProd, bs::network::Asset::Type::SpotXBT))
                    .arg(fxProd));


      // tx details
      if (side == bs::network::Side::Type::Sell) {
         dialogData.setValue("TotalSpent", UiUtils::displayQuantity(amount + fee, UiUtils::XbtCurrency));
      }
      else {
         dialogData.setValue("TotalReceived", UiUtils::displayQuantity(amount - fee, UiUtils::XbtCurrency));
      }

      dialogData.setValue("TransactionAmount", UiUtils::displayQuantity(amount, UiUtils::XbtCurrency));
      dialogData.setValue("NetworkFee", UiUtils::displayQuantity(fee, UiUtils::XbtCurrency));

      return dialogData;
   }

   bool isValidOffer(const Message_Offer &offer)
   {
      return offer.price() > 0 && offer.amount() > 0;
   }

   void copyOffer(const Offer &src, Message_Offer *dst)
   {
      dst->set_price(src.price);
      dst->set_amount(src.amount);
   }

} // namespace

struct OtcClientDeal
{
   bs::network::otc::Side side{};

   BinaryData settlementId;

   bs::core::wallet::TXSignRequest payin;
   bs::core::wallet::TXSignRequest payoutFallback;
   bs::core::wallet::TXSignRequest payout;

   bs::signer::RequestId payinReqId{};
   bs::signer::RequestId payoutFallbackReqId{};
   bs::signer::RequestId payoutReqId{};

   BinaryData payinSigned;
   BinaryData payoutFallbackSigned;
   BinaryData payoutSigned;

   BinaryData cpPubKey;

   int64_t amount{};
   int64_t fee{};
   int64_t price{};

   bool success{false};
   std::string errorMsg;

   static OtcClientDeal error(const std::string &msg)
   {
      OtcClientDeal result;
      result.errorMsg = msg;
      return result;
   }
};

OtcClient::OtcClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SignContainer> &signContainer
   , QObject *parent)
   : QObject (parent)
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , armory_(armory)
   , signContainer_(signContainer)
{
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletsSynchronized, this, [this] {
      auto leaf = ourSettlementLeaf();
      if (!leaf) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf");
         return;
      }

      leaf->getRootPubkey([this](const SecureBinaryData &ourPubKey) {
         if (ourPubKey.isNull()) {
            SPDLOG_LOGGER_ERROR(logger_, "invalid pubKey");
            return;
         }

         ourPubKey_ = ourPubKey;
      });
   });

   connect(signContainer.get(), &SignContainer::TXSigned, this, &OtcClient::onTxSigned);
}

OtcClient::~OtcClient() = default;

const Peer *OtcClient::peer(const std::string &peerId) const
{
   auto it = peers_.find(peerId);
   if (it == peers_.end()) {
      return nullptr;
   }
   return &it->second;
}

void OtcClient::setCurrentUserId(const std::string &userId)
{
   currentUserId_ = userId;
}

bool OtcClient::sendOffer(const Offer &offer, const std::string &peerId)
{
   SPDLOG_LOGGER_DEBUG(logger_, "send offer to {} (price: {}, amount: {})", peerId, offer.price, offer.amount);

   assert(offer.ourSide != otc::Side::Unknown);
   assert(offer.amount > 0);
   assert(offer.price > 0);

   auto peer = findPeer(peerId);
   if (!peer) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
      return false;
   }

   if (peer->state != State::Idle) {
      SPDLOG_LOGGER_ERROR(logger_, "can't send offer to '{}', peer should be in idle state", peerId);
      return false;
   }

   Message msg;
   if (offer.ourSide == otc::Side::Buy) {
      auto d = msg.mutable_buyer_offers();
      copyOffer(offer, d->mutable_offer());

      assert(!ourPubKey_.isNull());
      d->set_auth_address_buyer(ourPubKey_.toBinStr());
   } else {
      auto d = msg.mutable_seller_offers();
      copyOffer(offer, d->mutable_offer());
   }
   send(peer, msg);

   peer->offer = offer;
   changePeerState(peer, State::OfferSent);
   return true;
}

bool OtcClient::pullOrRejectOffer(const std::string &peerId)
{
   SPDLOG_LOGGER_DEBUG(logger_, "pull of reject offer from {}", peerId);

   auto peer = findPeer(peerId);
   if (!peer) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
      return false;
   }

   if (peer->state != State::OfferSent && peer->state != State::OfferRecv) {
      SPDLOG_LOGGER_ERROR(logger_, "can't pull offer from '{}', we should be in OfferSent or OfferRecv state", peerId);
      return false;
   }

   Message msg;
   msg.mutable_close();
   send(peer, msg);

   changePeerState(peer, State::Idle);
   return true;
}

bool OtcClient::acceptOffer(const bs::network::otc::Offer &offer, const std::string &peerId)
{
   SPDLOG_LOGGER_DEBUG(logger_, "accept offer from {} (price: {}, amount: {})", peerId, offer.price, offer.amount);

   assert(offer.ourSide != otc::Side::Unknown);
   assert(offer.amount > 0);
   assert(offer.price > 0);
   assert(ourPubKey_.getSize() == kPubKeySize);

   auto peer = findPeer(peerId);
   if (!peer) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
      return false;
   }

   if (peer->state != State::OfferRecv) {
      SPDLOG_LOGGER_ERROR(logger_, "can't accept offer from '{}', we should be in OfferRecv state", peerId);
      return false;
   }

   assert(offer == peer->offer);

   if (peer->offer.ourSide == otc::Side::Sell) {
      sendSellerAccepts(peer);
      return true;
   }

   // Need to get other details from seller first.
   // They should be available from Accept reply.
   Message msg;
   auto d = msg.mutable_buyer_accepts();
   copyOffer(offer, d->mutable_offer());
   d->set_auth_address_buyer(ourPubKey_.toBinStr());
   send(peer, msg);

   changePeerState(peer, State::WaitPayinInfo);
   return true;
}

bool OtcClient::updateOffer(const Offer &offer, const std::string &peerId)
{
   SPDLOG_LOGGER_DEBUG(logger_, "update offer from {} (price: {}, amount: {})", peerId, offer.price, offer.amount);

   assert(offer.ourSide != otc::Side::Unknown);
   assert(offer.amount > 0);
   assert(offer.price > 0);

   auto peer = findPeer(peerId);
   if (!peer) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
      return false;
   }

   if (peer->state != State::OfferRecv) {
      SPDLOG_LOGGER_ERROR(logger_, "can't pull offer from '{}', we should be in OfferRecv state", peerId);
      return false;
   }

   // Only price could be updated, amount and side must be the same
   assert(offer.amount == peer->offer.amount);
   assert(offer.ourSide == peer->offer.ourSide);

   peer->offer = offer;

   Message msg;
   if (offer.ourSide == otc::Side::Buy) {
      auto d = msg.mutable_buyer_offers();
      copyOffer(offer, d->mutable_offer());

      assert(!ourPubKey_.isNull());
      d->set_auth_address_buyer(ourPubKey_.toBinStr());
   } else {
      auto d = msg.mutable_seller_offers();
      copyOffer(offer, d->mutable_offer());
   }
   send(peer, msg);

   changePeerState(peer, State::OfferSent);
   return true;
}

void OtcClient::peerConnected(const std::string &peerId)
{
   Peer *oldPeer = findPeer(peerId);
   assert(!oldPeer);
   if (oldPeer) {
      oldPeer->state = State::Blacklisted;
      return;
   }

   peers_.emplace(peerId, Peer(peerId));
   emit peerUpdated(peerId);
}

void OtcClient::peerDisconnected(const std::string &peerId)
{
   peers_.erase(peerId);

   // TODO: Close tradings
   emit peerUpdated(peerId);
}

void OtcClient::processMessage(const std::string &peerId, const BinaryData &data)
{
   Peer *peer = findPeer(peerId);
   assert(peer);
   if (!peer) {
      SPDLOG_LOGGER_CRITICAL(logger_, "can't find peer '{}'", peerId);
      return;
   }

   if (peer->state == State::Blacklisted) {
      SPDLOG_LOGGER_DEBUG(logger_, "ignoring message from blacklisted peer '{}'", peerId);
      return;
   }

   Message message;
   bool result = message.ParseFromArray(data.getPtr(), int(data.getSize()));
   if (!result) {
      blockPeer("can't parse OTC message", peer);
      return;
   }

   switch (message.data_case()) {
      case Message::kBuyerOffers:
         processBuyerOffers(peer, message.buyer_offers());
         return;
      case Message::kSellerOffers:
         processSellerOffers(peer, message.seller_offers());
         return;
      case Message::kBuyerAccepts:
         processBuyerAccepts(peer, message.buyer_accepts());
         return;
      case Message::kSellerAccepts:
         processSellerAccepts(peer, message.seller_accepts());
         return;
      case Message::kBuyerAcks:
         processBuyerAcks(peer, message.buyer_acks());
         return;
      case Message::kClose:
         processClose(peer, message.close());
         return;
      case Message::DATA_NOT_SET:
         blockPeer("unknown or empty OTC message", peer);
         return;
   }

   SPDLOG_LOGGER_CRITICAL(logger_, "unknown response was detected!");
}

void OtcClient::processPbMessage(const std::string &data)
{
   ProxyTerminalPb::Response response;
   bool result = response.ParseFromString(data);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse message from PB");
      return;
   }

   switch (response.data_case()) {
      case ProxyTerminalPb::Response::kStartOtc:
         processPbStartOtc(response.start_otc());
         return;
      case ProxyTerminalPb::Response::kVerifyOtc:
         processPbVerifyOtc(response.verify_otc());
         return;
      case ProxyTerminalPb::Response::DATA_NOT_SET:
         SPDLOG_LOGGER_ERROR(logger_, "response from PB is invalid");
         return;
   }

   SPDLOG_LOGGER_CRITICAL(logger_, "unknown response was detected!");
}

void OtcClient::onTxSigned(unsigned reqId, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason)
{
   auto it = signRequestIds_.find(reqId);
   if (it == signRequestIds_.end()) {
      return;
   }
   const auto settlementId = std::move(it->second);
   signRequestIds_.erase(it);

   if (result != bs::error::ErrorCode::NoError) {
      // TODO: Cancel deal
      return;
   }

   auto dealIt = deals_.find(settlementId);
   if (dealIt == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unknown sign request");
      return;
   }
   OtcClientDeal *deal = dealIt->second.get();

   if (deal->payoutFallbackReqId == reqId) {
      SPDLOG_LOGGER_DEBUG(logger_, "fallback pay-out was succesfully signed, settlementId: {}", deal->settlementId.toHexStr());
      deal->payoutFallbackSigned = signedTX;
      trySendSignedTxs(deal);
   }

   if (deal->payinReqId == reqId) {
      SPDLOG_LOGGER_DEBUG(logger_, "pay-in was succesfully signed, settlementId: {}", deal->settlementId.toHexStr());
      deal->payinSigned = signedTX;
      trySendSignedTxs(deal);
   }

   if (deal->payoutReqId == reqId) {
      SPDLOG_LOGGER_DEBUG(logger_, "pay-out was succesfully signed, settlementId: {}", deal->settlementId.toHexStr());
      deal->payoutSigned = signedTX;
      trySendSignedTxs(deal);
   }
}

void OtcClient::processBuyerOffers(Peer *peer, const Message_BuyerOffers &msg)
{
   if (!isValidOffer(msg.offer())) {
      blockPeer("invalid offer", peer);
      return;
   }

   if (msg.auth_address_buyer().size() != kPubKeySize) {
      blockPeer("invalid auth_address_buyer in buyer offer", peer);
      return;
   }
   peer->authPubKey = msg.auth_address_buyer();

   switch (peer->state) {
      case State::Idle:
         peer->offer.ourSide = otc::Side::Sell;
         peer->offer.amount = msg.offer().amount();
         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         break;

      case State::OfferSent:
         if (peer->offer.ourSide != otc::Side::Sell) {
            blockPeer("unexpected side in counter-offer", peer);
            return;
         }
         if (peer->offer.amount != msg.offer().amount()) {
            blockPeer("invalid amount in counter-offer", peer);
            return;
         }

         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         break;

      case State::OfferRecv:
      case State::WaitPayinInfo:
      case State::SentPayinInfo:
         blockPeer("unexpected offer", peer);
         break;

      case State::Blacklisted:
         assert(false);
         break;
   }
}

void OtcClient::processSellerOffers(Peer *peer, const Message_SellerOffers &msg)
{
   if (!isValidOffer(msg.offer())) {
      blockPeer("invalid offer", peer);
      return;
   }

   switch (peer->state) {
      case State::Idle:
         peer->offer.ourSide = otc::Side::Buy;
         peer->offer.amount = msg.offer().amount();
         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         break;

      case State::OfferSent:
         if (peer->offer.ourSide != otc::Side::Buy) {
            blockPeer("unexpected side in counter-offer", peer);
            return;
         }
         if (peer->offer.amount != msg.offer().amount()) {
            blockPeer("invalid amount in counter-offer", peer);
            return;
         }

         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         break;

      case State::OfferRecv:
      case State::WaitPayinInfo:
      case State::SentPayinInfo:
         blockPeer("unexpected offer", peer);
         break;

      case State::Blacklisted:
         assert(false);
         break;
   }
}

void OtcClient::processBuyerAccepts(Peer *peer, const Message_BuyerAccepts &msg)
{
   if (peer->state != State::OfferSent || peer->offer.ourSide != otc::Side::Sell) {
      blockPeer("unexpected BuyerAccepts message, should be in OfferSent state and be seller", peer);
      return;
   }

   if (msg.offer().price() != peer->offer.price || msg.offer().amount() != peer->offer.amount) {
      blockPeer("wrong accepted price or amount in BuyerAccepts message", peer);
      return;
   }

   if (msg.auth_address_buyer().size() != kPubKeySize) {
      blockPeer("invalid auth_address in BuyerAccepts message", peer);
      return;
   }
   peer->authPubKey = msg.auth_address_buyer();

   sendSellerAccepts(peer);
}

void OtcClient::processSellerAccepts(Peer *peer, const Message_SellerAccepts &msg)
{
   if (msg.offer().price() != peer->offer.price || msg.offer().amount() != peer->offer.amount) {
      blockPeer("wrong accepted price or amount in SellerAccepts message", peer);
      return;
   }

   if (msg.settlement_id().size() != kSettlementIdSize) {
      blockPeer("invalid settlement_id in SellerAccepts message", peer);
      return;
   }
   const BinaryData settlementId(BinaryData(msg.settlement_id()));

   if (msg.auth_address_seller().size() != kPubKeySize) {
      blockPeer("invalid auth_address_seller in SellerAccepts message", peer);
      return;
   }
   peer->authPubKey = msg.auth_address_seller();

   if (msg.payin_tx_id().size() != kTxHashSize) {
      blockPeer("invalid payin_tx_id in SellerAccepts message", peer);
      return;
   }
   peer->payinTxIdFromSeller = BinaryData(msg.payin_tx_id());

   createRequests(settlementId, *peer, [this, settlementId, offer = peer->offer, peerId = peer->peerId](OtcClientDeal &&deal) {
      if (!deal.success) {
         SPDLOG_LOGGER_ERROR(logger_, "creating pay-out sign request fails: {}", deal.errorMsg);
         return;
      }

      auto peer = findPeer(peerId);
      if (!peer) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
         return;
      }

      if ((peer->state != State::WaitPayinInfo && peer->state != State::OfferSent) || peer->offer.ourSide != otc::Side::Buy) {
         blockPeer("unexpected SellerAccepts message, should be in WaitPayinInfo or OfferSent state and be buyer", peer);
         return;
      }

      if (offer != peer->offer) {
         SPDLOG_LOGGER_ERROR(logger_, "offer details have changed unexpectedly");
         return;
      }

      auto unsignedPayout = deal.payout.serializeState();
      deals_.emplace(settlementId, std::make_unique<OtcClientDeal>(std::move(deal)));

      Message msg;
      msg.mutable_buyer_acks()->set_settlement_id(settlementId.toBinStr());
      send(peer, msg);

      changePeerState(peer, State::Idle);

      ProxyTerminalPb::Request request;
      auto d = request.mutable_verify_otc();
      d->set_is_seller(false);
      d->set_price(peer->offer.price);
      d->set_amount(peer->offer.amount);
      d->set_settlement_id(settlementId.toBinStr());
      d->set_auth_address_buyer(ourPubKey_.toBinStr());
      d->set_auth_address_seller(peer->authPubKey.toBinStr());
      d->set_unsigned_payout(unsignedPayout.toBinStr());
      d->set_chat_id_buyer(currentUserId_);
      d->set_chat_id_seller(peer->peerId);
      emit sendPbMessage(request.SerializeAsString());

      *peer = Peer(peer->peerId);
      emit peerUpdated(peer->peerId);
   });
}

void OtcClient::processBuyerAcks(Peer *peer, const Message_BuyerAcks &msg)
{
   if (peer->state != State::SentPayinInfo || peer->offer.ourSide != otc::Side::Sell) {
      blockPeer("unexpected BuyerAcks message, should be in SentPayinInfo state and be seller", peer);
      return;
   }

   const auto settlementId = BinaryData(msg.settlement_id());

   const auto it = deals_.find(settlementId);
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unknown settlementId from BuyerAcks: {}", settlementId.toHexStr());
      return;
   }
   const auto &deal = it->second;
   assert(deal->success);

   ProxyTerminalPb::Request request;
   auto d = request.mutable_verify_otc();
   d->set_is_seller(true);
   d->set_price(peer->offer.price);
   d->set_amount(peer->offer.amount);
   d->set_settlement_id(settlementId.toBinStr());
   d->set_auth_address_buyer(peer->authPubKey.toBinStr());
   d->set_auth_address_seller(ourPubKey_.toBinStr());
   d->set_unsigned_payout(deal->payout.serializeState().toBinStr());
   d->set_chat_id_buyer(currentUserId_);
   d->set_chat_id_seller(peer->peerId);
   emit sendPbMessage(request.SerializeAsString());

   *peer = Peer(peer->peerId);
   emit peerUpdated(peer->peerId);
}

void OtcClient::processClose(Peer *peer, const Message_Close &msg)
{
   switch (peer->state) {
      case State::OfferSent:
      case State::OfferRecv:
         *peer = Peer(peer->peerId);
         emit peerUpdated(peer->peerId);
         break;

      case State::Idle:
      case State::WaitPayinInfo:
      case State::SentPayinInfo:
         blockPeer("unexpected close", peer);
         break;

      case State::Blacklisted:
         assert(false);
         break;
   }
}

void OtcClient::processPbStartOtc(const ProxyTerminalPb::Response_StartOtc &response)
{
   auto it = waitSettlementIds_.find(response.request_id());
   if (it == waitSettlementIds_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected StartOtc response: can't find request");
      return;
   }
   Peer peer = std::move(it->second);
   waitSettlementIds_.erase(it);

   const auto settlementId = BinaryData(response.settlement_id());

   createRequests(settlementId, peer, [this, settlementId, offer = peer.offer, peerId = peer.peerId](OtcClientDeal &&deal) {
      if (!deal.success) {
         SPDLOG_LOGGER_ERROR(logger_, "creating pay-in sign request fails: {}", deal.errorMsg);
         return;
      }

      auto peer = findPeer(peerId);
      if (!peer) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
         return;
      }

      if (offer.ourSide != otc::Side::Sell) {
         SPDLOG_LOGGER_ERROR(logger_, "can't send pay-in info, wrong side");
         return;
      }

      if (offer != peer->offer) {
         SPDLOG_LOGGER_ERROR(logger_, "offer details have changed unexpectedly");
         return;
      }

      auto payinTxId = deal.payin.txId();

      Message msg;
      auto d = msg.mutable_seller_accepts();
      copyOffer(peer->offer, d->mutable_offer());
      d->set_settlement_id(settlementId.toBinStr());
      d->set_auth_address_seller(ourPubKey_.toBinStr());
      d->set_payin_tx_id(payinTxId.toBinStr());
      send(peer, msg);

      changePeerState(peer, State::SentPayinInfo);

      deals_.emplace(settlementId, std::make_unique<OtcClientDeal>(deal));
   });
}

void OtcClient::processPbVerifyOtc(const ProxyTerminalPb::Response_VerifyOtc &response)
{
   const auto settlementId = BinaryData(response.settlement_id());

   auto it = deals_.find(response.settlement_id());
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unknown settlementId in VerifyOtc message");
      return;
   }
   auto deal = it->second.get();

   switch (deal->side) {
      case otc::Side::Buy: {
         assert(deal->payout.isValid());

         bs::core::wallet::SettlementData settlData;
         settlData.settlementId = settlementId;
         settlData.cpPublicKey = deal->cpPubKey;
         settlData.ownKeyFirst = true;

         auto payoutInfo = toPasswordDialogData(bs::network::Side::Type::Buy, deal->amount, deal->fee, deal->price);
         auto reqId = signContainer_->signSettlementPayoutTXRequest(deal->payout, settlData, payoutInfo);
         signRequestIds_[reqId] = settlementId;
         deal->payoutReqId = reqId;

         break;
      }
      case otc::Side::Sell: {
         assert(deal->payin.isValid());
         assert(deal->payoutFallback.isValid());

         bs::core::wallet::SettlementData settlData;
         settlData.settlementId = settlementId;
         settlData.cpPublicKey = deal->cpPubKey;
         settlData.ownKeyFirst = false;
         auto payoutFallbackInfo = toPasswordDialogData(bs::network::Side::Type::Sell, deal->amount, deal->fee, deal->price);
         auto reqId = signContainer_->signSettlementPayoutTXRequest(deal->payoutFallback, settlData, payoutFallbackInfo);
         signRequestIds_[reqId] = settlementId;
         deal->payoutFallbackReqId = reqId;

         auto payinInfo = toPasswordDialogData(bs::network::Side::Type::Sell, deal->amount, deal->fee, deal->price);
         reqId = signContainer_->signSettlementTXRequest(deal->payin, payinInfo);
         signRequestIds_[reqId] = settlementId;
         deal->payinReqId = reqId;

         break;
      }
      default:
         assert(false);
         break;
   }
}

void OtcClient::blockPeer(const std::string &reason, Peer *peer)
{
   SPDLOG_LOGGER_ERROR(logger_, "block broken peer '{}': {}, current state: {}"
      , peer->peerId, reason, toString(peer->state));
   peer->state = State::Blacklisted;
   emit peerUpdated(peer->peerId);
}

Peer *OtcClient::findPeer(const std::string &peerId)
{
   return const_cast<Peer*>(peer(peerId));
}

void OtcClient::send(Peer *peer, const Message &msg)
{
   assert(!peer->peerId.empty());
   emit sendMessage(peer->peerId, msg.SerializeAsString());
}

void OtcClient::createRequests(const BinaryData &settlementId, const Peer &peer, const OtcClientDealCb &cb)
{
   assert(peer.authPubKey.getSize() == kPubKeySize);
   assert(settlementId.getSize() == kSettlementIdSize);

   if (peer.offer.ourSide == bs::network::otc::Side::Buy) {
      assert(peer.payinTxIdFromSeller.getSize() == kTxHashSize);
   }

   auto leaf = ourSettlementLeaf();
   if (!leaf) {
      cb(OtcClientDeal::error("can't find settlement leaf"));
      return;
   }


   leaf->setSettlementID(settlementId, [this, settlementId, peer, cb](bool result) {
      if (!result) {
         cb(OtcClientDeal::error("setSettlementID failed"));
         return;
      }

      auto cbFee = [this, cb, peer, settlementId](float feePerByte) {
         if (feePerByte < 1) {
            cb(OtcClientDeal::error("invalid fees"));
            return;
         }

         auto hdWallet = walletsMgr_->getPrimaryWallet();
         if (!hdWallet) {
            cb(OtcClientDeal::error("can't find primary wallet"));
            return;
         }

         auto cbSettlAddr = [this, cb, peer, feePerByte, settlementId](const bs::Address &settlAddr) {
            if (settlAddr.isNull()) {
               cb(OtcClientDeal::error("invalid settl addr"));
               return;
            }

            auto wallet = ourBtcWallet();
            if (!wallet) {
               cb(OtcClientDeal::error("can't find BTC wallet"));
               return;
            }

            const auto changedCallback = nullptr;
            const bool isSegWitInputsOnly = true;
            const bool confirmedOnly = true;
            auto transaction = std::make_shared<TransactionData>(changedCallback, logger_, isSegWitInputsOnly, confirmedOnly);

            auto resetInputsCb = [this, cb, peer, transaction, settlAddr, feePerByte, settlementId]() {
               // resetInputsCb will be destroyed when returns, create one more callback to hold variables
               QMetaObject::invokeMethod(this, [this, cb, peer, transaction, settlAddr, feePerByte, settlementId] {
                  const double amount = peer.offer.amount / BTCNumericTypes::BalanceDivider;

                  OtcClientDeal result;
                  result.settlementId = settlementId;
                  result.cpPubKey = peer.authPubKey;
                  result.amount = peer.offer.amount;
                  result.price = peer.offer.price;

                  if (peer.offer.ourSide == bs::network::otc::Side::Sell) {
                     // Seller
                     auto index = transaction->RegisterNewRecipient();
                     assert(index == 0);
                     transaction->UpdateRecipient(0, amount, settlAddr);

                     if (!transaction->IsTransactionValid()) {
                        cb(OtcClientDeal::error("invalid pay-in transaction"));
                        return;
                     }

                     result.success = true;
                     result.side = otc::Side::Sell;
                     result.payin = transaction->createTXRequest();
                     auto payinTxId = result.payin.txId();
                     auto fallbackAddr = transaction->GetFallbackRecvAddress();
                     auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, payinTxId, amount);
                     result.payoutFallback = bs::SettlementMonitor::createPayoutTXRequest(
                        payinUTXO, fallbackAddr, feePerByte, armory_->topBlock());
                     result.fee = int64_t(result.payin.fee);
                     cb(std::move(result));
                     return;
                  }

                  // Buyer
                  result.success = true;
                  result.side = otc::Side::Buy;
                  auto outputAddr = transaction->GetFallbackRecvAddress();
                  auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, peer.payinTxIdFromSeller, amount);
                  result.payout = bs::SettlementMonitor::createPayoutTXRequest(
                     payinUTXO, outputAddr, feePerByte, armory_->topBlock());
                  result.fee = int64_t(result.payout.fee);
                  cb(std::move(result));
               }, Qt::QueuedConnection);
            };

            transaction->setFeePerByte(feePerByte);
            transaction->setWallet(wallet, armory_->topBlock(), false, resetInputsCb);
         };

         const bool myKeyFirst = (peer.offer.ourSide == bs::network::otc::Side::Buy);
         hdWallet->getSettlementPayinAddress(settlementId, peer.authPubKey, cbSettlAddr, myKeyFirst);
      };
      walletsMgr_->estimatedFeePerByte(2, cbFee, this);
   });
}

void OtcClient::sendSellerAccepts(Peer *peer)
{
   int requestId = genLocalUniqueId();
   waitSettlementIds_.emplace(requestId, *peer);

   ProxyTerminalPb::Request request;
   auto d = request.mutable_start_otc();
   d->set_request_id(requestId);
   emit sendPbMessage(request.SerializeAsString());

   QTimer::singleShot(kStartOtcTimeout, this, [this, requestId] {
      auto it = waitSettlementIds_.find(requestId);
      if (it != waitSettlementIds_.end()) {
         waitSettlementIds_.erase(it);
         SPDLOG_LOGGER_ERROR(logger_, "can't get settlementId from PB: timeout");

         // TODO: Report error that PB is not accessible to peer and cancel deal
      }
   });
}

std::shared_ptr<bs::sync::hd::SettlementLeaf> OtcClient::ourSettlementLeaf()
{
   // TODO: Use auth address from selection combobox

   auto wallet = walletsMgr_->getPrimaryWallet();
   if (!wallet) {
      SPDLOG_LOGGER_ERROR(logger_, "don't have primary wallet");
      return nullptr;
   }

   auto group = wallet->getGroup(bs::hd::BlockSettle_Settlement);
   if (!group) {
      SPDLOG_LOGGER_ERROR(logger_, "don't have settlement group");
      return nullptr;
   }

   auto leaves = group->getLeaves();
   if (leaves.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "empty settlements group");
      return nullptr;
   }

   auto leaf = std::dynamic_pointer_cast<bs::sync::hd::SettlementLeaf>(leaves.front());
   if (!leaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf");
      return nullptr;
   }

   return leaf;
}

std::shared_ptr<bs::sync::Wallet> OtcClient::ourBtcWallet()
{
   // TODO: Use wallet from selection combobox

   auto wallet = walletsMgr_->getPrimaryWallet();
   if (!wallet) {
      SPDLOG_LOGGER_ERROR(logger_, "don't have primary wallet");
      return nullptr;
   }

   auto group = wallet->getGroup(bs::hd::Bitcoin_test);
   if (!group) {
      SPDLOG_LOGGER_ERROR(logger_, "don't have bitcoin group");
      return nullptr;
   }

   auto leaves = group->getLeaves();
   if (leaves.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "empty bitcoin group");
      return nullptr;
   }

   return leaves.front();
}

void OtcClient::changePeerState(Peer *peer, bs::network::otc::State state)
{
   SPDLOG_LOGGER_DEBUG(logger_, "changing peer '{}' state from {} to {}"
      , peer->peerId, toString(peer->state), toString(state));
   peer->state = state;
   emit peerUpdated(peer->peerId);
}

void OtcClient::trySendSignedTxs(OtcClientDeal *deal)
{
   ProxyTerminalPb::Request request;
   auto d = request.mutable_broadcast_xbt();

   switch (deal->side) {
      case otc::Side::Buy:
         if (deal->payoutSigned.isNull()) {
            return;
         }
         d->set_signed_payout(deal->payoutSigned.toBinStr());
         break;
      case otc::Side::Sell:
         if (deal->payoutFallbackSigned.isNull() || deal->payinSigned.isNull()) {
            // Need to wait when both TX are signed
            return;
         }
         d->set_signed_payin(deal->payinSigned.toBinStr());
         d->set_signed_payout_fallback(deal->payoutFallbackSigned.toBinStr());
         break;
      default:
         assert(false);
   }

   d->set_settlement_id(deal->settlementId.toBinStr());
   emit sendPbMessage(request.SerializeAsString());
}
