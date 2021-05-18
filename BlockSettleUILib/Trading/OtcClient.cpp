/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OtcClient.h"

#include <QApplication>
#include <QFile>
#include <QTimer>

#include <spdlog/spdlog.h>

#include "AddressVerificator.h"
#include "AuthAddressManager.h"
#include "BtcUtils.h"
#include "CommonTypes.h"
#include "EncryptionUtils.h"
#include "OfflineSigner.h"
#include "ProtobufUtils.h"
#include "TradesUtils.h"
#include "UiUtils.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "ApplicationSettings.h"

#include "bs_proxy_terminal_pb.pb.h"
#include "otc.pb.h"

using namespace Blocksettle::Communication::Otc;
using namespace Blocksettle::Communication;
using namespace bs::network;
using namespace bs::network::otc;
using namespace bs::sync;

struct OtcClientDeal
{
   bs::network::otc::Side side{};

   std::string hdWalletId;
   std::string settlementId;
   bs::Address settlementAddr;

   bs::core::wallet::TXSignRequest payin;
   std::string payinState;
   bs::core::wallet::TXSignRequest payout;

   bs::signer::RequestId payinReqId{};
   bs::signer::RequestId payoutReqId{};

   BinaryData unsignedPayinHash;

   BinaryData signedTx;

   bs::Address ourAuthAddress;
   BinaryData cpPubKey;

   int64_t amount{};
   int64_t fee{};
   int64_t price{};

   bool success{false};
   std::string errorMsg;

   std::unique_ptr<AddressVerificator> addressVerificator;

   bs::network::otc::PeerPtr peer;
   ValidityHandle peerHandle;

   bs::Address cpAuthAddress() const
   {
      return bs::Address::fromPubKey(cpPubKey, AddressEntryType_P2WPKH);
   }

   bs::Address authAddress(bool forSeller) const
   {
      const bool weSell = (side == bs::network::otc::Side::Sell);
      if (forSeller == weSell) {
         return ourAuthAddress;
      } else {
         return cpAuthAddress();
      }
   }

   bool isRequestor() const { return side == bs::network::otc::Side::Sell; }
   bs::Address requestorAuthAddress() const { return authAddress(true); }
   bs::Address responderAuthAddress() const { return authAddress(false); }

   static OtcClientDeal error(const std::string &msg)
   {
      OtcClientDeal result;
      result.errorMsg = msg;
      return result;
   }
};

namespace {

   const int kContactIdSize = 12;

   const int kSettlementIdHexSize = 64;
   const int kTxHashSize = 32;
   const int kPubKeySize = 33;

   // Normally pay-in/pay-out timeout is detected using server's status update.
   // Use some delay to detect networking problems locally to prevent race.
   const auto kLocalTimeoutDelay = std::chrono::seconds(5);

   const auto kStartOtcTimeout = std::chrono::seconds(10);

   bs::sync::PasswordDialogData toPasswordDialogData(const OtcClientDeal &deal
      , const bs::core::wallet::TXSignRequest &signRequest
      , QDateTime timestamp, bool expandTxInfo)
   {
      double price = fromCents(deal.price);

      QString qtyProd = UiUtils::XbtCurrency;
      QString fxProd = QString::fromStdString("EUR");

      bs::sync::PasswordDialogData dialogData;

      dialogData.setValue(PasswordDialogData::Market, "XBT");

      dialogData.setValue(PasswordDialogData::ProductGroup, QObject::tr(bs::network::Asset::toString(bs::network::Asset::SpotXBT)));
      dialogData.setValue(PasswordDialogData::Security, "XBT/EUR");
      dialogData.setValue(PasswordDialogData::Product, "XBT");
      dialogData.setValue(PasswordDialogData::FxProduct, fxProd);

      dialogData.setValue(PasswordDialogData::Side, QObject::tr(bs::network::Side::toString(bs::network::Side::Type(deal.side))));
      dialogData.setValue(PasswordDialogData::Price, UiUtils::displayPriceXBT(price));

      dialogData.setValue(PasswordDialogData::Quantity, qApp->tr("%1 XBT")
                    .arg(UiUtils::displayAmount(deal.amount)));

      dialogData.setValue(PasswordDialogData::TotalValue, qApp->tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct((deal.amount / BTCNumericTypes::BalanceDivider) * price, fxProd, bs::network::Asset::Type::SpotXBT))
                    .arg(fxProd));

      dialogData.setValue(PasswordDialogData::SettlementAddress, deal.settlementAddr.display());
      dialogData.setValue(PasswordDialogData::SettlementId, deal.settlementId);

      dialogData.setValue(PasswordDialogData::IsDealer, !deal.isRequestor());
      dialogData.setValue(PasswordDialogData::RequesterAuthAddress, deal.requestorAuthAddress().display());
      dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, deal.isRequestor());
      dialogData.setValue(PasswordDialogData::ResponderAuthAddress, deal.responderAuthAddress().display());
      dialogData.setValue(PasswordDialogData::ResponderAuthAddressVerified, !deal.isRequestor());

      // Set timestamp that will be used by auth eid server to update timers.
      dialogData.setValue(PasswordDialogData::DurationTimestamp, static_cast<int>(timestamp.toSecsSinceEpoch()));

      dialogData.setValue(PasswordDialogData::ExpandTxInfo, expandTxInfo);

      return dialogData;
   }

   bs::sync::PasswordDialogData toPasswordDialogDataPayin(const OtcClientDeal &deal
      , const bs::core::wallet::TXSignRequest &signRequest
      , QDateTime timestamp, bool expandTxInfo)
   {
      auto dialogData = toPasswordDialogData(deal, signRequest, timestamp, expandTxInfo);
      dialogData.setValue(PasswordDialogData::SettlementPayInVisible, true);
      dialogData.setValue(PasswordDialogData::Title, QObject::tr("Settlement Pay-In"));
      dialogData.setValue(PasswordDialogData::DurationTotal, int(std::chrono::duration_cast<std::chrono::milliseconds>(otc::payinTimeout()).count()));
      dialogData.setValue(PasswordDialogData::DurationLeft, int(std::chrono::duration_cast<std::chrono::milliseconds>(otc::payinTimeout()).count()));
      return dialogData;
   }

   bs::sync::PasswordDialogData toPasswordDialogDataPayout(const OtcClientDeal &deal
      , const bs::core::wallet::TXSignRequest &signRequest
      , QDateTime timestamp, bool expandTxInfo)
   {
      auto dialogData = toPasswordDialogData(deal, signRequest, timestamp, expandTxInfo);
      dialogData.setValue(PasswordDialogData::SettlementPayOutVisible, true);
      dialogData.setValue(PasswordDialogData::Title, QObject::tr("Settlement Pay-Out"));
      dialogData.setValue(PasswordDialogData::DurationTotal, int(std::chrono::duration_cast<std::chrono::milliseconds>(otc::payoutTimeout()).count()));
      dialogData.setValue(PasswordDialogData::DurationLeft, int(std::chrono::duration_cast<std::chrono::milliseconds>(otc::payoutTimeout()).count()));
      return dialogData;
   }

   bool isValidOffer(const ContactMessage_Offer &offer)
   {
      return offer.price() > 0 && offer.amount() > 0;
   }

   void copyOffer(const Offer &src, ContactMessage_Offer *dst)
   {
      dst->set_price(src.price);
      dst->set_amount(src.amount);
   }

   void copyRange(const otc::Range &src, Otc::Range *dst)
   {
      dst->set_lower(src.lower);
      dst->set_upper(src.upper);
   }

   void copyRange(const Otc::Range&src, otc::Range *dst)
   {
      dst->lower = src.lower();
      dst->upper = src.upper();
   }

   PeerPtr findPeer(std::unordered_map<std::string, std::shared_ptr<bs::network::otc::Peer>> &map, const std::string &contactId)
   {
      auto it = map.find(contactId);
      return it == map.end() ? nullptr : it->second;
   }

} // namespace

OtcClient::OtcClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<WalletSignerContainer> &signContainer
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , const std::shared_ptr<ApplicationSettings>& applicationSettings
   , OtcClientParams params
   , QObject *parent)
   : QObject (parent)
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , armory_(armory)
   , signContainer_(signContainer)
   , authAddressManager_(authAddressManager)
   , utxoReservationManager_(utxoReservationManager)
   , applicationSettings_(applicationSettings)
   , params_(std::move(params))
{
//   connect(signContainer.get(), &SignContainer::TXSigned, this, &OtcClient::onTxSigned);
}

OtcClient::~OtcClient() = default;

PeerPtr OtcClient::contact(const std::string &contactId)
{
   return findPeer(contactMap_, contactId);
}

PeerPtr OtcClient::request(const std::string &contactId)
{
   return findPeer(requestMap_, contactId);
}

PeerPtr OtcClient::response(const std::string &contactId)
{
   return findPeer(responseMap_, contactId);
}

PeerPtr OtcClient::peer(const std::string &contactId, PeerType type)
{
   if (contactId.size() != kContactIdSize) {
      SPDLOG_LOGGER_DEBUG(logger_, "unexpected contact requested: {}", contactId);
   }

   switch (type)
   {
      case bs::network::otc::PeerType::Contact:
         return contact(contactId);
      case bs::network::otc::PeerType::Request:
         return request(contactId);
      case bs::network::otc::PeerType::Response:
         return response(contactId);
   }

   assert(false);
   return {};
}

void OtcClient::setOwnContactId(const std::string &contactId)
{
   ownContactId_ = contactId;
}

const std::string &OtcClient::ownContactId() const
{
   return ownContactId_;
}

bool OtcClient::sendQuoteRequest(const QuoteRequest &request)
{
   if (ownRequest_) {
      SPDLOG_LOGGER_ERROR(logger_, "own quote request was already sent");
      return false;
   }

   if (ownContactId_.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "own contact id is not set");
      return false;
   }

   ownRequest_ = std::make_unique<Peer>(ownContactId_, PeerType::Request);
   ownRequest_->request = request;
   ownRequest_->request.timestamp = QDateTime::currentDateTime();
   ownRequest_->isOwnRequest = true;
   scheduleCloseAfterTimeout(otc::publicRequestTimeout(), ownRequest_);

   Otc::PublicMessage msg;
   auto d = msg.mutable_request();
   d->set_sender_side(Otc::Side(request.ourSide));
   d->set_range(Otc::RangeType(request.rangeType));
   emit sendPublicMessage(BinaryData::fromString(msg.SerializeAsString()));

   updatePublicLists();

   return true;
}

bool OtcClient::sendQuoteResponse(const PeerPtr &peer, const QuoteResponse &quoteResponse)
{
   if (peer->state != State::Idle) {
      SPDLOG_LOGGER_ERROR(logger_, "can't send offer to '{}', peer should be in Idle state", peer->toString());
      return false;
   }

   if (!isSubRange(otc::getRange(peer->request.rangeType), quoteResponse.amount)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid range");
      return false;
   }

   changePeerState(peer, State::QuoteSent);
   scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
   peer->response = quoteResponse;

   Otc::ContactMessage msg;
   auto d = msg.mutable_quote_response();
   d->set_sender_side(Otc::Side(quoteResponse.ourSide));
   copyRange(quoteResponse.price, d->mutable_price());
   copyRange(quoteResponse.amount, d->mutable_amount());
   send(peer, msg);

   updatePublicLists();
   return true;
}

bool OtcClient::sendOffer(const PeerPtr &peer, const Offer &offer)
{
   SPDLOG_LOGGER_DEBUG(logger_, "send offer to {} (price: {}, amount: {})", peer->toString(), offer.price, offer.amount);

   if (!verifyOffer(offer)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid offer details");
      return false;
   }

   auto settlementLeaf = findSettlementLeaf(offer.authAddress);
   if (!settlementLeaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf with address '{}'", offer.authAddress);
      return false;
   }

   settlementLeaf->getRootPubkey([this, logger = logger_, peer, offer, handle = peer->validityFlag.handle()]
      (const SecureBinaryData &ourPubKey)
   {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
         return;
      }

      if (ourPubKey.getSize() != kPubKeySize) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid auth address root public key");
         return;
      }

      switch (peer->type) {
         case PeerType::Contact:
            if (peer->state != State::Idle) {
               SPDLOG_LOGGER_ERROR(logger_, "can't send offer to '{}', peer should be in idle state", peer->toString());
               return;
            }
            break;
         case PeerType::Request:
            SPDLOG_LOGGER_ERROR(logger_, "can't send offer to '{}'", peer->toString());
            return;
         case PeerType::Response:
            if (peer->state != State::QuoteRecv) {
               SPDLOG_LOGGER_ERROR(logger_, "can't send offer to '{}', peer should be in QuoteRecv state", peer->toString());
               return;
            }
            break;
      }

      peer->offer = offer;
      peer->ourAuthPubKey = ourPubKey;
      peer->isOurSideSentOffer = true;
      changePeerState(peer, State::OfferSent);
      scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);

      ContactMessage msg;
      if (offer.ourSide == otc::Side::Buy) {
         auto d = msg.mutable_buyer_offers();
         copyOffer(offer, d->mutable_offer());
         d->set_auth_address_buyer(peer->ourAuthPubKey.toBinStr());
      } else {
         auto d = msg.mutable_seller_offers();
         copyOffer(offer, d->mutable_offer());
      }
      send(peer, msg);
   });

   return true;
}

bool OtcClient::pullOrReject(const PeerPtr &peer)
{
   if (peer->isOwnRequest) {
      assert(peer == ownRequest_);

      SPDLOG_LOGGER_DEBUG(logger_, "pull own quote request");
      ownRequest_.reset();

      // This will remove everything when we pull public request.
      // We could keep current shield and show that our public request was pulled instead.
      responseMap_.clear();

      Otc::PublicMessage msg;
      msg.mutable_close();
      emit sendPublicMessage(BinaryData::fromString(msg.SerializeAsString()));

      updatePublicLists();
      return true;
   }

   switch (peer->state) {
      case State::QuoteSent:
      case State::OfferSent:
      case State::OfferRecv: {
         SPDLOG_LOGGER_DEBUG(logger_, "pull or reject offer from {}", peer->toString());

         ContactMessage msg;
         msg.mutable_close();
         send(peer, msg);

         switch (peer->type) {
            case PeerType::Contact:
               resetPeerStateToIdle(peer);
               break;
            case PeerType::Request:
               // Keep public request even if we reject it
               resetPeerStateToIdle(peer);
               // Need to call this as peer would be removed from "sent requests" list
               updatePublicLists();
               break;
            case PeerType::Response:
               // Remove peer from received responses if we reject it
               responseMap_.erase(peer->contactId);
               updatePublicLists();
               break;
         }

         return true;
      }

      case State::WaitBuyerSign:
      case State::WaitSellerSeal: {
         if (peer->state == State::WaitSellerSeal && peer->offer.ourSide == bs::network::otc::Side::Buy) {
            SPDLOG_LOGGER_ERROR(logger_, "buyer can't cancel deal while waiting payin sign", peer->toString());
            return false;
         }

         auto deal = deals_.at(peer->settlementId).get();
         ProxyTerminalPb::Request request;
         auto d = request.mutable_cancel();
         d->set_settlement_id(deal->settlementId);
         emit sendPbMessage(request.SerializeAsString());

         switch (peer->type) {
         case PeerType::Request:
            requestMap_.erase(peer->contactId);
            updatePublicLists();
            break;
         case PeerType::Response:
            responseMap_.erase(peer->contactId);
            updatePublicLists();
            break;
         }

         return true;
      }

      default: {
         SPDLOG_LOGGER_ERROR(logger_, "can't pull offer from '{}'", peer->toString());
         return false;
      }
   }
}

void OtcClient::setReservation(const PeerPtr &peer, bs::UtxoReservationToken&& reserv)
{
   reservedTokens_.erase(peer->contactId);
   reservedTokens_.insert({ peer->contactId , std::move(reserv) });
}

bs::UtxoReservationToken OtcClient::releaseReservation(const bs::network::otc::PeerPtr &peer)
{
   bs::UtxoReservationToken reservation;
   auto token = reservedTokens_.find(peer->contactId);
   if (token == reservedTokens_.end()) {
      return reservation;
   }

   reservation = std::move(token->second);
   reservedTokens_.erase(token);
   return reservation;
}

bool OtcClient::acceptOffer(const PeerPtr &peer, const bs::network::otc::Offer &offer)
{
   SPDLOG_LOGGER_DEBUG(logger_, "accept offer from {} (price: {}, amount: {})", peer->toString(), offer.price, offer.amount);

   if (!verifyOffer(offer)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid offer details");
      return false;
   }

   auto settlementLeaf = findSettlementLeaf(offer.authAddress);
   if (!settlementLeaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf with address '{}'", offer.authAddress);
      return false;
   }

   settlementLeaf->getRootPubkey([this, offer, peer, handle = peer->validityFlag.handle(), logger = logger_]
      (const SecureBinaryData &ourPubKey)
   {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
         return;
      }

      if (peer->state != State::OfferRecv) {
         SPDLOG_LOGGER_ERROR(logger_, "can't accept offer from '{}', we should be in OfferRecv state", peer->toString());
         return;
      }

      if (ourPubKey.getSize() != kPubKeySize) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid auth address root public key");
         return;
      }

      assert(offer == peer->offer);

      peer->offer = offer;
      peer->ourAuthPubKey = ourPubKey;

      if (peer->offer.ourSide == otc::Side::Sell) {
         sendSellerAccepts(peer);
         return;
      }

      // Need to get other details from seller first.
      // They should be available from Accept reply.
      ContactMessage msg;
      auto d = msg.mutable_buyer_accepts();
      copyOffer(offer, d->mutable_offer());
      d->set_auth_address_buyer(peer->ourAuthPubKey.toBinStr());
      send(peer, msg);

      changePeerState(peer, State::WaitPayinInfo);
   });

   return true;
}

bool OtcClient::updateOffer(const PeerPtr &peer, const Offer &offer)
{
   SPDLOG_LOGGER_DEBUG(logger_, "update offer from {} (price: {}, amount: {})", peer->toString(), offer.price, offer.amount);

   if (!verifyOffer(offer)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid offer details");
      return false;
   }

   auto settlementLeaf = findSettlementLeaf(offer.authAddress);
   if (!settlementLeaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf with address '{}'", offer.authAddress);
      return false;
   }

   settlementLeaf->getRootPubkey([this, offer, peer, handle = peer->validityFlag.handle(), logger = logger_]
      (const SecureBinaryData &ourPubKey)
   {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
         return;
      }

      if (peer->state != State::OfferRecv) {
         SPDLOG_LOGGER_ERROR(logger_, "can't pull offer from '{}', we should be in OfferRecv state", peer->toString());
         return;
      }

      if (ourPubKey.getSize() != kPubKeySize) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid auth address root public key");
         return;
      }

      // Only price could be updated, amount and side must be the same
      assert(offer.price != peer->offer.price);
      assert(offer.amount == peer->offer.amount);
      assert(offer.ourSide == peer->offer.ourSide);

      peer->offer = offer;
      peer->ourAuthPubKey = ourPubKey;

      ContactMessage msg;
      if (offer.ourSide == otc::Side::Buy) {
         auto d = msg.mutable_buyer_offers();
         copyOffer(offer, d->mutable_offer());

         d->set_auth_address_buyer(peer->ourAuthPubKey.toBinStr());
      } else {
         auto d = msg.mutable_seller_offers();
         copyOffer(offer, d->mutable_offer());
      }
      send(peer, msg);

      changePeerState(peer, State::OfferSent);
      scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
   });

   return true;
}

const PeerPtr &OtcClient::ownRequest() const
{
   return ownRequest_;
}

void OtcClient::contactConnected(const std::string &contactId)
{
   assert(!contact(contactId));
   contactMap_.emplace(contactId, std::make_shared<otc::Peer>(contactId, PeerType::Contact));
   emit publicUpdated();
}

void OtcClient::contactDisconnected(const std::string &contactId)
{
   const auto &peer = contactMap_.at(contactId);
   // Do not try to cancel deal waiting pay-in sign (will result in ban)
   if (peer->state == State::WaitBuyerSign) {
      pullOrReject(peer);
   }
   contactMap_.erase(contactId);
   reservedTokens_.erase(contactId);
   emit publicUpdated();
}

void OtcClient::processContactMessage(const std::string &contactId, const BinaryData &data)
{
   ContactMessage message;
   bool result = message.ParseFromArray(data.getPtr(), int(data.getSize()));
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse OTC message");
      return;
   }

   bs::network::otc::PeerPtr peer;
   switch (message.contact_type()) {
      case Otc::CONTACT_TYPE_PRIVATE:
         peer = contact(contactId);
         if (!peer) {
            SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", contactId);
            return;
         }

         if (peer->state == State::Blacklisted) {
            SPDLOG_LOGGER_DEBUG(logger_, "ignoring message from blacklisted peer '{}'", contactId);
            return;
         }
         break;

      case Otc::CONTACT_TYPE_PUBLIC_REQUEST:
         if (!ownRequest_) {
            SPDLOG_LOGGER_ERROR(logger_, "response is not expected");
            return;
         }

         peer = response(contactId);
         if (!peer) {
            auto result = responseMap_.emplace(contactId, std::make_shared<Peer>(contactId, PeerType::Response));
            peer = result.first->second;
            emit publicUpdated();
         }
         break;

      case Otc::CONTACT_TYPE_PUBLIC_RESPONSE:
         peer = request(contactId);
         if (!peer) {
            SPDLOG_LOGGER_ERROR(logger_, "request is not expected");
            return;
         }
         break;

      default:
         SPDLOG_LOGGER_ERROR(logger_, "unknown message type");
         return;
   }

   switch (message.data_case()) {
      case ContactMessage::kBuyerOffers:
         processBuyerOffers(peer, message.buyer_offers());
         return;
      case ContactMessage::kSellerOffers:
         processSellerOffers(peer, message.seller_offers());
         return;
      case ContactMessage::kBuyerAccepts:
         processBuyerAccepts(peer, message.buyer_accepts());
         return;
      case ContactMessage::kSellerAccepts:
         processSellerAccepts(peer, message.seller_accepts());
         return;
      case ContactMessage::kBuyerAcks:
         processBuyerAcks(peer, message.buyer_acks());
         return;
      case ContactMessage::kClose:
         processClose(peer, message.close());
         return;
      case ContactMessage::kQuoteResponse:
         processQuoteResponse(peer, message.quote_response());
         return;
      case ContactMessage::DATA_NOT_SET:
         blockPeer("unknown or empty OTC message", peer);
         return;
   }

   SPDLOG_LOGGER_CRITICAL(logger_, "unknown response was detected!");
}

void OtcClient::processPbMessage(const ProxyTerminalPb::Response &response)
{
   switch (response.data_case()) {
      case ProxyTerminalPb::Response::kStartOtc:
         processPbStartOtc(response.start_otc());
         return;
      case ProxyTerminalPb::Response::kUpdateOtcState:
         processPbUpdateOtcState(response.update_otc_state());
         return;
      case ProxyTerminalPb::Response::DATA_NOT_SET:
         SPDLOG_LOGGER_ERROR(logger_, "response from PB is invalid");
         return;
      default:
         // if not processed - not OTC message. not error
         break;
   }
}

void OtcClient::processPublicMessage(QDateTime timestamp, const std::string &contactId, const BinaryData &data)
{
   assert(!ownContactId_.empty());
   if (contactId == ownContactId_) {
      return;
   }

   Otc::PublicMessage msg;
   bool result = msg.ParseFromArray(data.getPtr(), int(data.getSize()));
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "parsing public OTC message failed");
      return;
   }

   switch (msg.data_case()) {
      case Otc::PublicMessage::kRequest:
         processPublicRequest(timestamp, contactId, msg.request());
         return;
      case Otc::PublicMessage::kClose:
         processPublicClose(timestamp, contactId, msg.close());
         return;
      case Otc::PublicMessage::DATA_NOT_SET:
         SPDLOG_LOGGER_ERROR(logger_, "invalid public request detected");
         return;
   }

   SPDLOG_LOGGER_CRITICAL(logger_, "unknown public message was detected!");
}

void OtcClient::onTxSigned(unsigned reqId, BinaryData signedTX
   , bs::error::ErrorCode result, const std::string &errorReason)
{
   auto it = signRequestIds_.find(reqId);
   if (it == signRequestIds_.end()) {
      return;
   }
   const auto settlementId = std::move(it->second);
   signRequestIds_.erase(it);

   auto dealIt = deals_.find(settlementId);
   if (dealIt == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unknown sign request");
      return;
   }
   OtcClientDeal *deal = dealIt->second.get();

   if (result == bs::error::ErrorCode::NoError) {
      if (deal->payinReqId == reqId) {
         SPDLOG_LOGGER_DEBUG(logger_, "pay-in was succesfully signed, settlementId: {}", deal->settlementId);
         deal->signedTx = signedTX;

         ProxyTerminalPb::Request request;
         auto d = request.mutable_seal_payin_validity();
         d->set_settlement_id(deal->settlementId);
         emit sendPbMessage(request.SerializeAsString());
      }

      if (deal->payoutReqId == reqId) {
         SPDLOG_LOGGER_DEBUG(logger_, "pay-out was succesfully signed, settlementId: {}", deal->settlementId);
         deal->signedTx = signedTX;
         trySendSignedTx(deal);
      }
      return;
   }
   else {
      SPDLOG_LOGGER_ERROR(logger_, "sign error: {}", errorReason);
   }

   if (!deal->peerHandle.isValid()) {
      SPDLOG_LOGGER_ERROR(logger_, "peer was destroyed");
      return;
   }
   auto peer = deal->peer;
   peer->activeSettlementId.clear();
   pullOrReject(peer);
}

void OtcClient::processBuyerOffers(const PeerPtr &peer, const ContactMessage_BuyerOffers &msg)
{
   if (!isValidOffer(msg.offer())) {
      blockPeer("invalid offer", peer);
      return;
   }

   if (msg.auth_address_buyer().size() != kPubKeySize) {
      blockPeer("invalid auth_address_buyer in buyer offer", peer);
      return;
   }
   peer->authPubKey = BinaryData::fromString(msg.auth_address_buyer());

   switch (peer->state) {
      case State::Idle:
         peer->offer.ourSide = otc::Side::Sell;
         peer->offer.amount = msg.offer().amount();
         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
         break;

      case State::QuoteSent:
         peer->offer.ourSide = otc::Side::Sell;
         peer->offer.amount = msg.offer().amount();
         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
         break;

      case State::QuoteRecv:
         SPDLOG_LOGGER_ERROR(logger_, "not implemented");
         return;

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
         scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
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

void OtcClient::processSellerOffers(const PeerPtr &peer, const ContactMessage_SellerOffers &msg)
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
         scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
         break;

      case State::QuoteSent:
         peer->offer.ourSide = otc::Side::Buy;
         peer->offer.amount = msg.offer().amount();
         peer->offer.price = msg.offer().price();
         changePeerState(peer, State::OfferRecv);
         scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
         break;

      case State::QuoteRecv:
         SPDLOG_LOGGER_ERROR(logger_, "not implemented");
         return;

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
         scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
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

void OtcClient::processBuyerAccepts(const PeerPtr &peer, const ContactMessage_BuyerAccepts &msg)
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
   peer->authPubKey = BinaryData::fromString(msg.auth_address_buyer());

   sendSellerAccepts(peer);
}

void OtcClient::processSellerAccepts(const PeerPtr &peer, const ContactMessage_SellerAccepts &msg)
{
   if (msg.offer().price() != peer->offer.price || msg.offer().amount() != peer->offer.amount) {
      blockPeer("wrong accepted price or amount in SellerAccepts message", peer);
      return;
   }

   if (msg.settlement_id().size() != kSettlementIdHexSize) {
      blockPeer("invalid settlement_id in SellerAccepts message", peer);
      return;
   }
   const auto &settlementId = msg.settlement_id();

   if (msg.auth_address_seller().size() != kPubKeySize) {
      blockPeer("invalid auth_address_seller in SellerAccepts message", peer);
      return;
   }
   peer->authPubKey = BinaryData::fromString(msg.auth_address_seller());

   if (msg.payin_tx_id().size() != kTxHashSize) {
      blockPeer("invalid payin_tx_id in SellerAccepts message", peer);
      return;
   }
   peer->payinTxIdFromSeller = BinaryData::fromString(msg.payin_tx_id());

   createBuyerRequest(settlementId, peer, [this, peer, settlementId, offer = peer->offer
      , handle = peer->validityFlag.handle(), logger = logger_] (OtcClientDeal &&deal)
   {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
         return;
      }

      if (!deal.success) {
         SPDLOG_LOGGER_ERROR(logger_, "creating pay-out sign request fails: {}", deal.errorMsg);
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

      deal.peer = peer;
      deal.peerHandle = std::move(handle);
      deals_.emplace(settlementId, std::make_unique<OtcClientDeal>(std::move(deal)));

      ContactMessage msg;
      msg.mutable_buyer_acks()->set_settlement_id(settlementId);
      send(peer, msg);

      changePeerState(peer, otc::State::WaitVerification);

      ProxyTerminalPb::Request request;
      auto d = request.mutable_verify_otc();
      d->set_is_seller(false);
      d->set_price(peer->offer.price);
      d->set_amount(peer->offer.amount);
      d->set_settlement_id(settlementId);
      d->set_auth_address_buyer(peer->ourAuthPubKey.toBinStr());
      d->set_auth_address_seller(peer->authPubKey.toBinStr());
      d->set_unsigned_tx(unsignedPayout.SerializeAsString());
      d->set_payin_tx_hash(peer->payinTxIdFromSeller.toBinStr());
      d->set_chat_id_buyer(ownContactId_);
      d->set_chat_id_seller(peer->contactId);
      emit sendPbMessage(request.SerializeAsString());
   });
}

void OtcClient::processBuyerAcks(const PeerPtr &peer, const ContactMessage_BuyerAcks &msg)
{
   if (peer->state != State::SentPayinInfo || peer->offer.ourSide != otc::Side::Sell) {
      blockPeer("unexpected BuyerAcks message, should be in SentPayinInfo state and be seller", peer);
      return;
   }

   const auto &settlementId = msg.settlement_id();

   const auto it = deals_.find(settlementId);
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unknown settlementId from BuyerAcks: {}", settlementId);
      return;
   }
   const auto &deal = it->second;
   assert(deal->success);

   changePeerState(peer, otc::State::WaitVerification);

   ProxyTerminalPb::Request request;

   auto d = request.mutable_verify_otc();
   d->set_is_seller(true);
   d->set_price(peer->offer.price);
   d->set_amount(peer->offer.amount);
   d->set_settlement_id(settlementId);
   d->set_auth_address_buyer(peer->authPubKey.toBinStr());
   d->set_auth_address_seller(peer->ourAuthPubKey.toBinStr());
   d->set_unsigned_tx(deal->payinState);

   d->set_payin_tx_hash(deal->unsignedPayinHash.toBinStr());

   d->set_chat_id_seller(ownContactId_);
   d->set_chat_id_buyer(peer->contactId);
   emit sendPbMessage(request.SerializeAsString());
}

void OtcClient::processClose(const PeerPtr &peer, const ContactMessage_Close &msg)
{
   switch (peer->state) {
      case State::QuoteSent:
      case State::QuoteRecv:
      case State::OfferSent:
      case State::OfferRecv:
      case State::WaitPayinInfo: {
         if (peer->type == PeerType::Response) {
            SPDLOG_LOGGER_DEBUG(logger_, "remove active response because peer have sent close message");
            responseMap_.erase(peer->contactId);
            updatePublicLists();
            return;
         }

         resetPeerStateToIdle(peer);
         if (peer->type != PeerType::Contact) {
            updatePublicLists();
         }
         break;
      }

      case State::Idle:
         // Could happen if both sides press cancel at the same time
         break;

      case State::WaitVerification:
      case State::WaitBuyerSign:
      case State::WaitSellerSeal:
      case State::WaitSellerSign:
         // After sending verification details both sides should use PB only
         SPDLOG_LOGGER_DEBUG(logger_, "ignoring unexpected close request");
         break;

      case State::SentPayinInfo: {
         blockPeer("unexpected close", peer);
         break;
      }

      case State::Blacklisted: {
         assert(false);
         break;
      }
   }
}

void OtcClient::processQuoteResponse(const PeerPtr &peer, const ContactMessage_QuoteResponse &msg)
{
   if (!ownRequest_) {
      SPDLOG_LOGGER_ERROR(logger_, "own request is not available");
      return;
   }

   if (peer->type != PeerType::Response) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected request");
      return;
   }

   changePeerState(peer, State::QuoteRecv);
   scheduleCloseAfterTimeout(otc::negotiationTimeout(), peer);
   peer->response.ourSide = otc::switchSide(otc::Side(msg.sender_side()));
   copyRange(msg.price(), &peer->response.price);
   copyRange(msg.amount(), &peer->response.amount);

   updatePublicLists();
}

void OtcClient::processPublicRequest(QDateTime timestamp, const std::string &contactId, const PublicMessage_Request &msg)
{
   auto range = otc::RangeType(msg.range());
   if (range < otc::firstRangeValue(params_.env) || range > otc::lastRangeValue(params_.env)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid range");
      return;
   }

   requestMap_.erase(contactId);
   auto result = requestMap_.emplace(contactId, std::make_shared<Peer>(contactId, PeerType::Request));
   const auto &peer = result.first->second;

   peer->request.ourSide = otc::switchSide(otc::Side(msg.sender_side()));
   peer->request.rangeType = range;
   peer->request.timestamp = timestamp;

   updatePublicLists();
}

void OtcClient::processPublicClose(QDateTime timestamp, const std::string &contactId, const PublicMessage_Close &msg)
{
   requestMap_.erase(contactId);

   updatePublicLists();
}

void OtcClient::processPbStartOtc(const ProxyTerminalPb::Response_StartOtc &response)
{
   auto it = waitSettlementIds_.find(response.request_id());
   if (it == waitSettlementIds_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected StartOtc response: can't find request");
      return;
   }
   auto handle = std::move(it->second.handle);
   auto peer = it->second.peer;
   waitSettlementIds_.erase(it);

   const auto &settlementId = response.settlement_id();

   if (!handle.isValid()) {
      SPDLOG_LOGGER_ERROR(logger_, "peer was destroyed");
      return;
   }

   createSellerRequest(settlementId, peer, [this, peer, settlementId, offer = peer->offer
      , handle = peer->validityFlag.handle(), logger = logger_](OtcClientDeal &&deal)
   {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
         return;
      }

      if (!deal.success) {
         SPDLOG_LOGGER_ERROR(logger_, "creating pay-in sign request fails: {}", deal.errorMsg);
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

      ContactMessage msg;
      auto d = msg.mutable_seller_accepts();
      copyOffer(peer->offer, d->mutable_offer());
      d->set_settlement_id(settlementId);
      d->set_auth_address_seller(peer->ourAuthPubKey.toBinStr());
      d->set_payin_tx_id(deal.unsignedPayinHash.toBinStr());
      d->set_payin_tx(deal.payin.serializeState().SerializeAsString());
      send(peer, msg);

      deal.peer = peer;
      deal.peerHandle = std::move(handle);
      deal.payinState = deal.payin.serializeState().SerializeAsString();
      deals_.emplace(settlementId, std::make_unique<OtcClientDeal>(std::move(deal)));

      changePeerState(peer, State::SentPayinInfo);
   });
}

void OtcClient::processPbUpdateOtcState(const ProxyTerminalPb::Response_UpdateOtcState &response)
{
   auto it = deals_.find(response.settlement_id());
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "unknown settlementId in UpdateOtcState message");
      return;
   }
   auto deal = it->second.get();

   switch (response.state()) {
      case ProxyTerminalPb::OTC_STATE_WAIT_SELLER_SIGN:
         if (deal->side == otc::Side::Sell) {
            trySendSignedTx(deal);
         }
         break;
      default:
         break;
   }

   if (!deal->peerHandle.isValid()) {
      SPDLOG_LOGGER_ERROR(logger_, "peer was destroyed");
      return;
   }
   auto peer = deal->peer;

   SPDLOG_LOGGER_DEBUG(logger_, "change OTC trade state to: {}, settlementId: {}"
      , response.settlement_id(), ProxyTerminalPb::OtcState_Name(response.state()));

   auto timestamp = QDateTime::fromMSecsSinceEpoch(response.timestamp_ms());

   switch (response.state()) {
      case ProxyTerminalPb::OTC_STATE_FAILED: {
         switch (peer->state) {
            case State::WaitVerification:
            case State::WaitBuyerSign:
            case State::WaitSellerSeal:
            case State::WaitSellerSign:
               break;
            default:
               SPDLOG_LOGGER_ERROR(logger_, "unexpected state update request");
               return;
         }

         SPDLOG_LOGGER_ERROR(logger_, "OTC trade failed: {}", response.error_msg());
         emit peerError(peer, PeerErrorType::Rejected, &response.error_msg());

         resetPeerStateToIdle(peer);
         break;
      }

      case ProxyTerminalPb::OTC_STATE_WAIT_BUYER_SIGN: {
         if (peer->state != State::WaitVerification) {
            SPDLOG_LOGGER_ERROR(logger_, "unexpected state update request");
            return;
         }

         if (deal->side == otc::Side::Buy) {
            assert(deal->payout.isValid());

            bs::core::wallet::SettlementData settlData;
            settlData.settlementId = BinaryData::CreateFromHex(deal->settlementId);
            settlData.cpPublicKey = deal->cpPubKey;
            settlData.ownKeyFirst = true;

            auto payoutInfo = toPasswordDialogDataPayout(*deal, deal->payout, timestamp, expandTxDialog());
            auto reqId = signContainer_->signSettlementPayoutTXRequest(deal->payout, settlData, payoutInfo);
            signRequestIds_[reqId] = deal->settlementId;
            deal->payoutReqId = reqId;
            verifyAuthAddresses(deal);
            peer->activeSettlementId = BinaryData::fromString(deal->settlementId);
         }

         changePeerState(peer, State::WaitBuyerSign);

         QTimer::singleShot(payoutTimeout() + kLocalTimeoutDelay, this, [this, peer, handle = peer->validityFlag.handle()] {
            if (!handle.isValid() || peer->state != State::WaitBuyerSign) {
               return;
            }
            emit peerError(peer, PeerErrorType::Timeout, nullptr);
            resetPeerStateToIdle(peer);
         });
         break;
      }

      case ProxyTerminalPb::OTC_STATE_WAIT_SELLER_SEAL: {
         if (peer->state != State::WaitBuyerSign) {
            SPDLOG_LOGGER_ERROR(logger_, "unexpected state update request");
            return;
         }

         if (deal->side == otc::Side::Sell) {
            assert(deal->payin.isValid());

            const auto &authLeaf = walletsMgr_->getAuthWallet();
            if (!authLeaf) {
               SPDLOG_LOGGER_ERROR(logger_, "can't find auth wallet");
               return;
            }
            signContainer_->setSettlCP(authLeaf->walletId(), deal->unsignedPayinHash, BinaryData::CreateFromHex(deal->settlementId), deal->cpPubKey);
            signContainer_->setSettlAuthAddr(authLeaf->walletId(), BinaryData::CreateFromHex(deal->settlementId), deal->ourAuthAddress);

            auto payinInfo = toPasswordDialogDataPayin(*deal, deal->payin, timestamp, expandTxDialog());
            auto reqId = signContainer_->signSettlementTXRequest(deal->payin, payinInfo);
            signRequestIds_[reqId] = deal->settlementId;
            deal->payinReqId = reqId;
            verifyAuthAddresses(deal);
            peer->activeSettlementId = BinaryData::fromString(deal->settlementId);
         }

         changePeerState(peer, State::WaitSellerSeal);

         QTimer::singleShot(payinTimeout() + kLocalTimeoutDelay, this, [this, peer, handle = peer->validityFlag.handle()] {
            if (!handle.isValid() || peer->state != State::WaitSellerSeal) {
               return;
            }
            emit peerError(peer, PeerErrorType::Timeout, nullptr);
            resetPeerStateToIdle(peer);
         });
         break;
      }

      case ProxyTerminalPb::OTC_STATE_WAIT_SELLER_SIGN: {
         if (peer->state != State::WaitSellerSeal) {
            SPDLOG_LOGGER_ERROR(logger_, "unexpected state update request");
            return;
         }
         changePeerState(peer, State::WaitSellerSign);
         break;
      }

      case ProxyTerminalPb::OTC_STATE_CANCELLED: {
         if (peer->state != State::WaitBuyerSign && peer->state != State::WaitSellerSeal) {
            SPDLOG_LOGGER_ERROR(logger_, "unexpected state update request");
            return;
         }
         emit peerError(peer, PeerErrorType::Canceled, nullptr);
         switch (peer->type) {
         case PeerType::Contact:
            resetPeerStateToIdle(peer);
            break;
         case PeerType::Request:
            requestMap_.erase(peer->contactId);
            updatePublicLists();
            break;
         case PeerType::Response:
            responseMap_.erase(peer->contactId);
            updatePublicLists();
            break;
         }

         break;
      }

      case ProxyTerminalPb::OTC_STATE_SUCCEED: {
         if (peer->state != State::WaitSellerSign) {
            SPDLOG_LOGGER_ERROR(logger_, "unexpected state update request");
            return;
         }

         resetPeerStateToIdle(peer);
         break;
      }

      default: {
         SPDLOG_LOGGER_ERROR(logger_, "unexpected new state value: {}", int(response.state()));
         break;
      }
   }
}

bool OtcClient::verifyOffer(const Offer &offer) const
{
   assert(offer.ourSide != otc::Side::Unknown);
   assert(offer.amount > 0);
   assert(offer.price > 0);
   assert(!offer.hdWalletId.empty());
   assert(bs::Address::fromAddressString(offer.authAddress).isValid());

   if (!offer.recvAddress.empty()) {
      auto offerRecvAddress = bs::Address::fromAddressString(offer.recvAddress);
      assert(offerRecvAddress.isValid());
      auto wallet = walletsMgr_->getWalletByAddress(offerRecvAddress);

      if (!wallet || wallet->type() != bs::core::wallet::Type::Bitcoin) {
         SPDLOG_LOGGER_CRITICAL(logger_, "invalid receiving address selected for OTC, selected address: {}", offer.recvAddress);
         return false;
      }

      auto hdWalletRecv = walletsMgr_->getHDRootForLeaf(wallet->walletId());
      if (!hdWalletRecv || hdWalletRecv->walletId() != offer.hdWalletId) {
         SPDLOG_LOGGER_CRITICAL(logger_, "invalid receiving address selected for OTC (invalid hd wallet), selected address: {}", offer.recvAddress);
         return false;
      }
   }

   if (!walletsMgr_->getHDWalletById(offer.hdWalletId)) {
      SPDLOG_LOGGER_ERROR(logger_, "hd wallet not found: {}", offer.hdWalletId);
      return false;
   }
   if (offer.ourSide == bs::network::otc::Side::Buy && !offer.inputs.empty()) {
      SPDLOG_LOGGER_CRITICAL(logger_, "inputs must be empty for sell");
      return false;
   }
   for (const auto &input : offer.inputs) {
      auto address = bs::Address::fromUTXO(input);
      auto wallet = walletsMgr_->getWalletByAddress(address);
      if (!wallet) {
         SPDLOG_LOGGER_CRITICAL(logger_, "wallet not found for UTXO from address: {}", address.display());
         return false;
      }
      const auto hdWalletId = walletsMgr_->getHDRootForLeaf(wallet->walletId())->walletId();
      if (hdWalletId != offer.hdWalletId) {
         SPDLOG_LOGGER_CRITICAL(logger_, "invalid UTXO, hdWalletId: {}, expected hdWalletId: {}"
            , hdWalletId, offer.hdWalletId);
         return false;
      }
   }

   // utxoReservationManager_ is not available in unit tests
   if (utxoReservationManager_) {
      auto minXbtAmount = bs::tradeutils::minXbtAmount(utxoReservationManager_->feeRatePb());
      if (offer.amount < static_cast<int64_t>(minXbtAmount.GetValue())) {
         SPDLOG_LOGGER_ERROR(logger_, "amount is too low: {}, min amount: {}", offer.amount, minXbtAmount.GetValue());
         return false;
      }
   }

   return true;
}

void OtcClient::blockPeer(const std::string &reason, const PeerPtr &peer)
{
   SPDLOG_LOGGER_ERROR(logger_, "block broken peer '{}': {}", peer->toString(), reason);
   changePeerState(peer, State::Blacklisted);
   emit peerUpdated(peer);
}

void OtcClient::send(const PeerPtr &peer, ContactMessage &msg)
{
   assert(!peer->contactId.empty());
   msg.set_contact_type(Otc::ContactType(peer->type));
   emit sendContactMessage(peer->contactId, BinaryData::fromString(msg.SerializeAsString()));
}

void OtcClient::createSellerRequest(const std::string &settlementId, const PeerPtr &peer, const OtcClientDealCb &cb)
{
   assert(peer->authPubKey.getSize() == kPubKeySize);
   assert(settlementId.size() == kSettlementIdHexSize);
   assert(!peer->offer.authAddress.empty());
   assert(peer->offer.ourSide == bs::network::otc::Side::Sell);

   auto primaryHdWallet = walletsMgr_->getPrimaryWallet();
   if (!primaryHdWallet) {
      cb(OtcClientDeal::error("can't find primary wallet"));
      return;
   }

   auto targetHdWallet = walletsMgr_->getHDWalletById(peer->offer.hdWalletId);
   if (!targetHdWallet) {
      cb(OtcClientDeal::error(fmt::format("can't find wallet: {}", peer->offer.hdWalletId)));
      return;
   }

   auto group = targetHdWallet->getGroup(targetHdWallet->getXBTGroupType());
   std::vector<std::shared_ptr<bs::sync::Wallet>> xbtWallets;
   if (!targetHdWallet->canMixLeaves()) {
      assert(peer->offer.walletPurpose);
      xbtWallets.push_back(group->getLeaf(*peer->offer.walletPurpose));
   }
   else {
      xbtWallets = group->getAllLeaves();
   }

   if (xbtWallets.empty()) {
      cb(OtcClientDeal::error("can't find XBT wallets"));
      return;
   }

   bs::tradeutils::PayinArgs args;
   initTradesArgs(args, peer, settlementId);
   args.fixedInputs = peer->offer.inputs;
   args.inputXbtWallets = xbtWallets;

   auto payinCb = bs::tradeutils::PayinResultCb([this, cb, peer, settlementId
      , targetHdWallet, handle = peer->validityFlag.handle(), logger = logger_]
      (bs::tradeutils::PayinResult payin)
   {
      QMetaObject::invokeMethod(this, [this, cb, targetHdWallet, settlementId, handle, logger, peer, payin = std::move(payin)]
         {
         if (!handle.isValid()) {
            SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
            return;
         }

         if (!payin.success) {
            SPDLOG_LOGGER_ERROR(logger, "creating unsigned payin failed: {}", payin.errorMsg);
            cb(OtcClientDeal::error("invalid pay-in transaction"));
            return;
         }

         peer->settlementId = settlementId;

         auto result = std::make_shared<OtcClientDeal>();
         result->settlementId = settlementId;
         result->settlementAddr = payin.settlementAddr;
         result->ourAuthAddress = bs::Address::fromAddressString(peer->offer.authAddress);
         result->cpPubKey = peer->authPubKey;
         result->amount = peer->offer.amount;
         result->price = peer->offer.price;
         result->hdWalletId = targetHdWallet->walletId();
         result->success = true;
         result->side = otc::Side::Sell;
         result->payin = std::move(payin.signRequest);
         result->payin.expiredTimestamp = std::chrono::system_clock::now() + otc::payinTimeout();

         result->unsignedPayinHash = payin.payinHash;

         result->fee = int64_t(result->payin.fee);

         const auto &cbResolveSpenders = [result, cb, this](bs::error::ErrorCode errCode
            , const Codec_SignerState::SignerState &state)
         {
            if (errCode == bs::error::ErrorCode::NoError) {
               result->payin.armorySigner_.deserializeState(state);
            }
            cb(std::move(*result));
         };
         signContainer_->resolvePublicSpenders(result->payin, cbResolveSpenders);
      });
   });

   bs::tradeutils::createPayin(std::move(args), std::move(payinCb));
}

void OtcClient::createBuyerRequest(const std::string &settlementId, const PeerPtr &peer, const OtcClient::OtcClientDealCb &cb)
{
   assert(peer->authPubKey.getSize() == kPubKeySize);
   assert(settlementId.size() == kSettlementIdHexSize);
   assert(!peer->offer.authAddress.empty());
   assert(peer->offer.ourSide == bs::network::otc::Side::Buy);
   assert(peer->payinTxIdFromSeller.getSize() == kTxHashSize);

   auto targetHdWallet = walletsMgr_->getHDWalletById(peer->offer.hdWalletId);
   if (!targetHdWallet) {
      cb(OtcClientDeal::error(fmt::format("can't find wallet: {}", peer->offer.hdWalletId)));
      return;
   }

   auto group = targetHdWallet->getGroup(targetHdWallet->getXBTGroupType());
   std::vector<std::shared_ptr<bs::sync::Wallet>> xbtWallets;
   if (!targetHdWallet->canMixLeaves()) {
      assert(peer->offer.walletPurpose);
      xbtWallets.push_back(group->getLeaf(*peer->offer.walletPurpose));
   }
   else {
      xbtWallets = group->getAllLeaves();
   }

   bs::tradeutils::PayoutArgs args;
   initTradesArgs(args, peer, settlementId);
   args.payinTxId = peer->payinTxIdFromSeller;
   if (!peer->offer.recvAddress.empty()) {
      args.recvAddr = bs::Address::fromAddressString(peer->offer.recvAddress);
   }
   args.outputXbtWallet = xbtWallets.front();

   auto payoutCb = bs::tradeutils::PayoutResultCb([this, cb, peer, settlementId, targetHdWallet, handle = peer->validityFlag.handle(), logger = logger_]
      (bs::tradeutils::PayoutResult payout)
   {
      QMetaObject::invokeMethod(this, [cb, targetHdWallet, settlementId, handle, peer, logger, payout = std::move(payout)] {
         if (!handle.isValid()) {
            SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
            return;
         }

         peer->settlementId = settlementId;

         OtcClientDeal result;
         result.settlementId = settlementId;
         result.settlementAddr = payout.settlementAddr;
         result.ourAuthAddress = bs::Address::fromAddressString(peer->offer.authAddress);
         result.cpPubKey = peer->authPubKey;
         result.amount = peer->offer.amount;
         result.price = peer->offer.price;
         result.hdWalletId = targetHdWallet->walletId();
         result.success = true;
         result.side = otc::Side::Buy;
         result.payout = std::move(payout.signRequest);
         result.fee = int64_t(result.payout.fee);
         cb(std::move(result));
      });
   });

   bs::tradeutils::createPayout(std::move(args), std::move(payoutCb));
};

void OtcClient::sendSellerAccepts(const PeerPtr &peer)
{
   int requestId = genLocalUniqueId();
   waitSettlementIds_.emplace(requestId, SettlementIdRequest{peer, peer->validityFlag.handle()});

   ProxyTerminalPb::Request request;
   auto d = request.mutable_start_otc();
   d->set_request_id(requestId);
   emit sendPbMessage(request.SerializeAsString());

   QTimer::singleShot(kStartOtcTimeout, this, [this, requestId, peer, handle = peer->validityFlag.handle()] {
      if (!handle.isValid()) {
         return;
      }
      auto it = waitSettlementIds_.find(requestId);
      if (it == waitSettlementIds_.end()) {
         return;
      }
      waitSettlementIds_.erase(it);
      SPDLOG_LOGGER_ERROR(logger_, "can't get settlementId from PB: timeout");
      emit peerError(peer, PeerErrorType::Timeout, nullptr);
      pullOrReject(peer);
   });
}

std::shared_ptr<bs::sync::hd::SettlementLeaf> OtcClient::findSettlementLeaf(const std::string &ourAuthAddress)
{
   return walletsMgr_->getSettlementLeaf(bs::Address::fromAddressString(ourAuthAddress));
}

void OtcClient::changePeerStateWithoutUpdate(const PeerPtr &peer, State state)
{
   SPDLOG_LOGGER_DEBUG(logger_, "changing peer '{}' state from {} to {}"
      , peer->toString(), toString(peer->state), toString(state));
   peer->state = state;
   peer->stateTimestamp = std::chrono::steady_clock::now();

   switch (state)
   {
      case bs::network::otc::State::Idle:
      case bs::network::otc::State::Blacklisted:
         releaseReservation(peer);
         break;
      default:
         break;
   }
}

void OtcClient::changePeerState(const PeerPtr &peer, bs::network::otc::State state)
{
   changePeerStateWithoutUpdate(peer, state);
   emit peerUpdated(peer);
}

void OtcClient::resetPeerStateToIdle(const PeerPtr &peer)
{
   if (!peer->activeSettlementId.empty()) {
      signContainer_->CancelSignTx(peer->activeSettlementId);
      peer->activeSettlementId.clear();
   }

   changePeerStateWithoutUpdate(peer, State::Idle);
   auto request = std::move(peer->request);
   if (!peer->settlementId.empty()) {
      deals_.erase(peer->settlementId);
   }
   *peer = Peer(peer->contactId, peer->type);
   peer->request = std::move(request);
   emit peerUpdated(peer);
}

void OtcClient::scheduleCloseAfterTimeout(std::chrono::milliseconds timeout, const PeerPtr &peer)
{
   // Use PreciseTimer to prevent time out earlier than expected
   QTimer::singleShot(timeout, Qt::PreciseTimer, this, [this, peer, oldState = peer->state, handle = peer->validityFlag.handle(), timeout] {
      if (!handle.isValid() || peer->state != oldState) {
         return;
      }
      // Prevent closing from some old state if peer had switched back and forth
      auto diff = std::chrono::steady_clock::now() - peer->stateTimestamp;
      if (diff >= timeout) {
         pullOrReject(peer);
      }
   });
}

void OtcClient::trySendSignedTx(OtcClientDeal *deal)
{
   ProxyTerminalPb::Request request;
   auto d = request.mutable_process_tx();
   d->set_signed_tx(deal->signedTx.toBinStr());
   d->set_settlement_id(deal->settlementId);
   emit sendPbMessage(request.SerializeAsString());

   setComments(deal);
}

void OtcClient::verifyAuthAddresses(OtcClientDeal *deal)
{
   if (!authAddressManager_) {
      SPDLOG_LOGGER_DEBUG(logger_, "authAddressManager_ is not set, auth address verification skipped");
      return;
   }

   auto verificatorCb = [this, logger = logger_, handle = deal->peerHandle, settlementId = deal->settlementId
         , requesterAuthAddr = deal->requestorAuthAddress(), responderAuthAddr = deal->responderAuthAddress()]
         (const bs::Address &address, AddressVerificationState state)
   {
      QMetaObject::invokeMethod(qApp, [this, logger, handle, state, address, settlementId, requesterAuthAddr, responderAuthAddr] {
         if (!handle.isValid()) {
            SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
            return;
         }

         SPDLOG_LOGGER_DEBUG(logger_, "counterparty's auth address ({}) status: {}", address.display(), to_string(state));
         if (state != AddressVerificationState::Verified) {
            return;
         }

         bs::sync::PasswordDialogData dialogData;
         dialogData.setValue(PasswordDialogData::SettlementId, settlementId);
         if (address == requesterAuthAddr) {
            dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, true);
         } else if (address == responderAuthAddr) {
            dialogData.setValue(PasswordDialogData::ResponderAuthAddressVerified, true);
         } else {
            SPDLOG_LOGGER_ERROR(logger, "unexpected auth address");
            return;
         }
         signContainer_->updateDialogData(dialogData);
      });
   };

   if (authAddressVerificationRequired(deal)) {
      deal->addressVerificator = std::make_unique<AddressVerificator>(logger_, armory_, verificatorCb);
      deal->addressVerificator->SetBSAddressList(authAddressManager_->GetBSAddresses());
      deal->addressVerificator->addAddress(deal->cpAuthAddress());
      deal->addressVerificator->startAddressVerification();
   } else {
      verificatorCb(deal->cpAuthAddress(), AddressVerificationState::Verified);
   }
}

void OtcClient::setComments(OtcClientDeal *deal)
{
   auto hdWallet = walletsMgr_->getHDWalletById(deal->hdWalletId);
   auto group = hdWallet ? hdWallet->getGroup(hdWallet->getXBTGroupType()) : nullptr;
   auto leaves = group ? group->getAllLeaves() : std::vector<std::shared_ptr<bs::sync::Wallet>>();
   for (const auto & leaf : leaves) {
      const double price = bs::network::otc::fromCents(deal->price);
      auto comment = fmt::format("{} XBT/EUR @ {} (OTC)"
         , bs::network::otc::toString(deal->side), UiUtils::displayPriceXBT(price).toStdString());
      leaf->setTransactionComment(deal->signedTx, comment);
   }
}

void OtcClient::updatePublicLists()
{
   contacts_.clear();
   contacts_.reserve(contactMap_.size());
   for (auto &item : contactMap_) {
      contacts_.push_back(item.second);
   }

   requests_.clear();
   requests_.reserve(requestMap_.size() + (ownRequest_ ? 1 : 0));
   if (ownRequest_) {
      requests_.push_back(ownRequest_);
   }
   for (auto &item : requestMap_) {
      requests_.push_back(item.second);
   }

   responses_.clear();
   responses_.reserve(responseMap_.size());
   for (auto &item : responseMap_) {
      responses_.push_back(item.second);
   }

   emit publicUpdated();
}

void OtcClient::initTradesArgs(bs::tradeutils::Args &args, const PeerPtr &peer, const std::string &settlementId)
{
   args.amount = bs::XBTAmount(static_cast<int64_t>(peer->offer.amount));
   args.settlementId = BinaryData::CreateFromHex(settlementId);
   args.walletsMgr = walletsMgr_;
   args.ourAuthAddress = bs::Address::fromAddressString(peer->offer.authAddress);
   args.cpAuthPubKey = peer->authPubKey;
   args.armory = armory_;
   args.signContainer = signContainer_;
   // utxoReservationManager_ is null in unit tests
   if (utxoReservationManager_) {
      args.feeRatePb_ = utxoReservationManager_->feeRatePb();
   }
}

bool OtcClient::expandTxDialog() const
{
   return applicationSettings_->get<bool>(
            ApplicationSettings::DetailedSettlementTxDialogByDefault);
}

bool OtcClient::authAddressVerificationRequired(OtcClientDeal *deal) const
{
   if (!applicationSettings_) {
      return true;
   }
   const auto tier1XbtLimit = applicationSettings_->get<uint64_t>(
      ApplicationSettings::SubmittedAddressXbtLimit);
   return static_cast<uint64_t>(deal->amount) > tier1XbtLimit;
}
