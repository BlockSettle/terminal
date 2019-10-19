#include "OtcClient.h"

#include <QApplication>
#include <QFile>
#include <QTimer>
#include <spdlog/spdlog.h>

#include "AddressVerificator.h"
#include "AuthAddressManager.h"
#include "BtcUtils.h"
#include "CoinSelection.h"
#include "CommonTypes.h"
#include "EncryptionUtils.h"
#include "OfflineSigner.h"
#include "ProtobufUtils.h"
#include "SelectedTransactionInputs.h"
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
using namespace bs::sync;

struct OtcClientDeal
{
   bs::network::otc::Side side{};

   std::string hdWalletId;
   std::string settlementId;
   bs::Address settlementAddr;

   bs::core::wallet::TXSignRequest payin;
   bs::core::wallet::TXSignRequest payout;

   bs::signer::RequestId payinReqId{};
   bs::signer::RequestId payoutReqId{};

   BinaryData payinTxId;
   BinaryData signedTx;

   bs::Address ourAuthAddress;
   BinaryData cpPubKey;

   int64_t amount{};
   int64_t fee{};
   int64_t price{};

   bool success{false};
   std::string errorMsg;

   std::unique_ptr<AddressVerificator> addressVerificator;

   otc::Peer *peer{};
   ValidityHandle peerHandle;

   bs::Address authAddress(bool isSeller) const
   {
      const bool weSell = (side == bs::network::otc::Side::Sell);
      if (isSeller == weSell) {
         return ourAuthAddress;
      } else {
         return bs::Address::fromPubKey(cpPubKey).display();
      }
   }

   bool isRequestor() const { return side == bs::network::otc::Side::Sell; }
   bs::Address requestorAuthAddress() const { return authAddress(isRequestor()); }
   bs::Address responderAuthAddress() const { return authAddress(!isRequestor()); }

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

   bs::sync::PasswordDialogData toPasswordDialogData(const OtcClientDeal &deal, const bs::core::wallet::TXSignRequest &signRequest)
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

      dialogData.setValue(PasswordDialogData::RequesterAuthAddress, deal.requestorAuthAddress().display());
      dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, deal.isRequestor());

      dialogData.setValue(PasswordDialogData::ResponderAuthAddress, deal.responderAuthAddress().display());
      dialogData.setValue(PasswordDialogData::ResponderAuthAddressVerified, !deal.isRequestor());

      return dialogData;
   }

   bs::sync::PasswordDialogData toPasswordDialogDataPayin(const OtcClientDeal &deal, const bs::core::wallet::TXSignRequest &signRequest)
   {
      auto dialogData = toPasswordDialogData(deal, signRequest);
      dialogData.setValue(PasswordDialogData::SettlementPayInVisible, true);
      dialogData.setValue(PasswordDialogData::Title, QObject::tr("Settlement Pay-In"));
      dialogData.setValue(PasswordDialogData::DurationTotal, int(std::chrono::duration_cast<std::chrono::milliseconds>(otc::payinTimeout()).count()));
      dialogData.setValue(PasswordDialogData::DurationLeft, int(std::chrono::duration_cast<std::chrono::milliseconds>(otc::payinTimeout()).count()));
      return dialogData;
   }

   bs::sync::PasswordDialogData toPasswordDialogDataPayout(const OtcClientDeal &deal, const bs::core::wallet::TXSignRequest &signRequest)
   {
      auto dialogData = toPasswordDialogData(deal, signRequest);
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

   Peer *findPeer(std::unordered_map<std::string, bs::network::otc::Peer> &map, const std::string &contactId)
   {
      auto it = map.find(contactId);
      return it == map.end() ? nullptr : &it->second;
   }

} // namespace

OtcClient::OtcClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SignContainer> &signContainer
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , OtcClientParams params
   , QObject *parent)
   : QObject (parent)
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , armory_(armory)
   , signContainer_(signContainer)
   , authAddressManager_(authAddressManager)
   , params_(std::move(params))
{
   connect(signContainer.get(), &SignContainer::TXSigned, this, &OtcClient::onTxSigned);
}

OtcClient::~OtcClient() = default;

Peer *OtcClient::contact(const std::string &contactId)
{
   return findPeer(contactMap_, contactId);
}

Peer *OtcClient::request(const std::string &contactId)
{
   return findPeer(requestMap_, contactId);
}

Peer *OtcClient::response(const std::string &contactId)
{
   return findPeer(responseMap_, contactId);
}

Peer *OtcClient::peer(const std::string &contactId, PeerType type)
{
   if (contactId.size() != kContactIdSize) {
      SPDLOG_LOGGER_DEBUG(logger_, "unexpected contact requested: {}");
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
   return nullptr;
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
   scheduleCloseAfterTimeout(otc::publicRequestTimeout(), ownRequest_.get());

   Otc::PublicMessage msg;
   auto d = msg.mutable_request();
   d->set_sender_side(Otc::Side(request.ourSide));
   d->set_range(Otc::RangeType(request.rangeType));
   emit sendPublicMessage(msg.SerializeAsString());

   updatePublicLists();

   return true;
}

bool OtcClient::sendQuoteResponse(Peer *peer, const QuoteResponse &quoteResponse)
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

bool OtcClient::sendOffer(Peer *peer, const Offer &offer)
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

bool OtcClient::pullOrReject(Peer *peer)
{
   if (peer->isOwnRequest) {
      assert(peer == ownRequest_.get());

      SPDLOG_LOGGER_DEBUG(logger_, "pull own quote request");
      ownRequest_.reset();

      // This will remove everything when we pull public request.
      // We could keep current shield and show that our public request was pulled instead.
      responseMap_.clear();

      Otc::PublicMessage msg;
      msg.mutable_close();
      emit sendPublicMessage(msg.SerializeAsString());

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

bool OtcClient::saveOfflineRequest(Peer *peer, const std::string &path)
{
   if (!peer || !peer->isWaitingForOfflineSign()) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected request to save offline sign request");
      return false;
   }

   auto it = deals_.find(peer->settlementId);
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find active deal details");
      return false;
   }
   auto deal = it->second.get();

   SPDLOG_LOGGER_DEBUG(logger_, "save sign request from offline wallet to '{}'");

   deal->payin.offlineFilePath = path;
   auto reqId = signContainer_->signTXRequest(deal->payin);
   signRequestIds_[reqId] = deal->settlementId;
   deal->payinReqId = reqId;
   return true;
}

bool OtcClient::loadOfflineRequest(Peer *peer, const std::string &path)
{
   if (!peer || !peer->isWaitingForOfflineSign()) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected request to load offline sign request");
      return false;
   }

   auto it = deals_.find(peer->settlementId);
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find active deal details");
      return false;
   }
   auto deal = it->second.get();

   QFile f(QString::fromStdString(path));
   bool result = f.open(QIODevice::ReadOnly);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't open file ('{}') to load signed offline request", path);
      return false;
   }
   auto results = ParseOfflineTXFile(f.readAll().toStdString());
   if (results.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "loading signed offline request failed from '{}'", path);
      return false;
   }
   if (results.size() != 1 || results.front().prevStates.size() != 1) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid signed offline request in '{}'", path);
      return false;
   }

   // TODO: Verify that we have correct signed request

   SPDLOG_LOGGER_DEBUG(logger_, "pay-in was succesfully signed (using offline wallet), settlementId: {}", deal->settlementId);
   deal->signedTx = std::move(results.front().prevStates.front());
   return true;
}

bool OtcClient::sendOfflineRequest(Peer *peer)
{
   if (!peer || !peer->isWaitingForOfflineSign()) {
      SPDLOG_LOGGER_ERROR(logger_, "unexpected request to send offline sign request");
      return false;
   }

   auto it = deals_.find(peer->settlementId);
   if (it == deals_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find active deal details");
      return false;
   }
   auto deal = it->second.get();

   if (deal->signedTx.isNull()) {
      SPDLOG_LOGGER_ERROR(logger_, "offline sign request was not loaded yet");
      return false;
   }

   SPDLOG_LOGGER_DEBUG(logger_, "send was succesfully signed, settlementId: {}", deal->settlementId);

   ProxyTerminalPb::Request request;
   auto d = request.mutable_seal_payin_validity();
   d->set_settlement_id(deal->settlementId);
   emit sendPbMessage(request.SerializeAsString());
   return true;
}

bool OtcClient::acceptOffer(Peer *peer, const bs::network::otc::Offer &offer)
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

bool OtcClient::updateOffer(Peer *peer, const Offer &offer)
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

Peer *OtcClient::ownRequest() const
{
   return ownRequest_.get();
}

unsigned OtcClient::feeTargetBlockCount()
{
   return 2;
}

uint64_t OtcClient::estimatePayinFeeWithoutChange(const std::vector<UTXO> &inputs, float feePerByte)
{
   // add workaround for computeSizeAndFee (it can't compute exact v-size before signing,
   // sometimes causing "fee not met" error for 1 sat/byte)
   if (feePerByte >= 1.0f && feePerByte < 1.01f) {
      feePerByte = 1.01f;
   }

   std::map<unsigned, std::shared_ptr<ScriptRecipient>> recipientsMap;
   // Use some fake settlement address as the only recipient
   auto recipient = bs::Address(CryptoPRNG::generateRandom(32), AddressEntryType_P2WSH);
   // Select some random amount
   recipientsMap[0] = recipient.getRecipient(bs::XBTAmount{ uint64_t{1000} });

   auto inputsCopy = bs::Address::decorateUTXOsCopy(inputs);
   PaymentStruct payment(recipientsMap, 0, feePerByte, 0);
   uint64_t result = bs::Address::getFeeForMaxVal(inputsCopy, payment.size_, feePerByte);
   return result;
}

void OtcClient::contactConnected(const std::string &contactId)
{
   assert(!contact(contactId));
   contactMap_.emplace(contactId, otc::Peer(contactId, PeerType::Contact));
   emit publicUpdated();
}

void OtcClient::contactDisconnected(const std::string &contactId)
{
   const auto peer = &contactMap_.at(contactId);

   switch (peer->state) {
      case State::WaitBuyerSign:
      case State::WaitSellerSeal:
         // Notify PB that contact was disconnected and deal could be canceled
         pullOrReject(peer);
         break;
      default:
         // No need to notify PB in other cases:
         // WaitVerification - temporary state (perhaps about 5 seconds) and PB would detect pay-out timeout
         // WaitSellerSign - temporary state (about 10 seconds) and PB would detect pay-in timeout
         break;
   }

   contactMap_.erase(contactId);

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

   Peer *peer{};
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
            auto result = responseMap_.emplace(contactId, Peer(contactId, PeerType::Response));
            peer = &result.first->second;
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

void OtcClient::onTxSigned(unsigned reqId, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason)
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

   if (!deal->peerHandle.isValid()) {
      SPDLOG_LOGGER_ERROR(logger_, "peer was destroyed");
      return;
   }
   auto peer = deal->peer;

   peer->activeSettlementId.clear();

   if (result != bs::error::ErrorCode::NoError) {
      pullOrReject(peer);
      return;
   }

   if (deal->payinReqId == reqId) {
      if (peer->state != State::WaitSellerSeal) {
         SPDLOG_LOGGER_ERROR(logger_, "unexpected pay-in sign");
         return;
      }

      if (peer->sellFromOffline) {
         // Wait for explicit call to loadOfflineRequest from user
         return;
      }

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
}

void OtcClient::processBuyerOffers(Peer *peer, const ContactMessage_BuyerOffers &msg)
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

void OtcClient::processSellerOffers(Peer *peer, const ContactMessage_SellerOffers &msg)
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

void OtcClient::processBuyerAccepts(Peer *peer, const ContactMessage_BuyerAccepts &msg)
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

void OtcClient::processSellerAccepts(Peer *peer, const ContactMessage_SellerAccepts &msg)
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
   peer->authPubKey = msg.auth_address_seller();

   if (msg.payin_tx_id().size() != kTxHashSize) {
      blockPeer("invalid payin_tx_id in SellerAccepts message", peer);
      return;
   }
   peer->payinTxIdFromSeller = BinaryData(msg.payin_tx_id());

   createRequests(settlementId, peer, [this, peer, settlementId, offer = peer->offer
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
      d->set_unsigned_tx(unsignedPayout.toBinStr());
      d->set_payin_hash(peer->payinTxIdFromSeller.toBinStr());
      d->set_chat_id_buyer(ownContactId_);
      d->set_chat_id_seller(peer->contactId);
      emit sendPbMessage(request.SerializeAsString());
   });
}

void OtcClient::processBuyerAcks(Peer *peer, const ContactMessage_BuyerAcks &msg)
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
   d->set_unsigned_tx(deal->payin.serializeState().toBinStr());
   d->set_payin_hash(deal->payinTxId.toBinStr());
   d->set_chat_id_seller(ownContactId_);
   d->set_chat_id_buyer(peer->contactId);
   emit sendPbMessage(request.SerializeAsString());
}

void OtcClient::processClose(Peer *peer, const ContactMessage_Close &msg)
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

void OtcClient::processQuoteResponse(Peer *peer, const ContactMessage_QuoteResponse &msg)
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
   auto result = requestMap_.emplace(contactId, Peer(contactId, PeerType::Request));
   auto peer = &result.first->second;

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

   createRequests(settlementId, peer, [this, peer, settlementId, offer = peer->offer
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
      d->set_payin_tx_id(deal.payinTxId.toBinStr());
      send(peer, msg);

      deal.peer = peer;
      deal.peerHandle = std::move(handle);
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

   if (!deal->peerHandle.isValid()) {
      SPDLOG_LOGGER_ERROR(logger_, "peer was destroyed");
      return;
   }
   auto peer = deal->peer;

   SPDLOG_LOGGER_DEBUG(logger_, "change OTC trade state to: {}, settlementId: {}"
      , response.settlement_id(), ProxyTerminalPb::OtcState_Name(response.state()));

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

            auto payoutInfo = toPasswordDialogDataPayout(*deal, deal->payout);
            auto reqId = signContainer_->signSettlementPayoutTXRequest(deal->payout, settlData, payoutInfo);
            signRequestIds_[reqId] = deal->settlementId;
            deal->payoutReqId = reqId;
            verifyAuthAddresses(deal);
            peer->activeSettlementId = deal->settlementId;
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

            if (peer->sellFromOffline) {

            } else {
               auto payinInfo = toPasswordDialogDataPayin(*deal, deal->payin);
               auto reqId = signContainer_->signSettlementTXRequest(deal->payin, payinInfo);
               signRequestIds_[reqId] = deal->settlementId;
               deal->payinReqId = reqId;
               verifyAuthAddresses(deal);
               peer->activeSettlementId = deal->settlementId;
            }
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

         if (deal->side == otc::Side::Sell) {
            trySendSignedTx(deal);
         }
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
   assert(bs::Address(offer.authAddress).isValid());

   if (!offer.recvAddress.empty()) {
      assert(bs::Address(offer.recvAddress).isValid());
      auto wallet = walletsMgr_->getWalletByAddress(offer.recvAddress);

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

   return true;
}

void OtcClient::blockPeer(const std::string &reason, Peer *peer)
{
   SPDLOG_LOGGER_ERROR(logger_, "block broken peer '{}': {}", peer->toString(), reason);
   changePeerState(peer, State::Blacklisted);
   emit peerUpdated(peer);
}

void OtcClient::send(Peer *peer, ContactMessage &msg)
{
   assert(!peer->contactId.empty());
   msg.set_contact_type(Otc::ContactType(peer->type));
   emit sendContactMessage(peer->contactId, msg.SerializeAsString());
}

void OtcClient::createRequests(const std::string &settlementId, Peer *peer, const OtcClientDealCb &cb)
{
   assert(peer->authPubKey.getSize() == kPubKeySize);
   assert(settlementId.size() == kSettlementIdHexSize);
   if (peer->offer.ourSide == bs::network::otc::Side::Buy) {
      assert(peer->payinTxIdFromSeller.getSize() == kTxHashSize);
   }
   assert(!peer->offer.authAddress.empty());

   auto leaf = findSettlementLeaf(peer->offer.authAddress);
   if (!leaf) {
      cb(OtcClientDeal::error("can't find settlement leaf"));
      return;
   }

   leaf->setSettlementID(SecureBinaryData::CreateFromHex(settlementId), [this, settlementId, peer, cb, handle = peer->validityFlag.handle()
      , logger = logger_](bool result)
   {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
         return;
      }

      if (!result) {
         cb(OtcClientDeal::error("setSettlementID failed"));
         return;
      }

      auto cbFee = [this, cb, peer, settlementId, handle, logger = logger_](float feePerByte) {
         if (!handle.isValid()) {
            SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
            return;
         }

         if (feePerByte < 1) {
            cb(OtcClientDeal::error("invalid fees"));
            return;
         }

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

         auto cbSettlAddr = [this, cb, peer, feePerByte, settlementId, targetHdWallet, handle, logger = logger_]
            (const bs::Address &settlAddr)
         {
            if (!handle.isValid()) {
               SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
               return;
            }

            if (settlAddr.isNull()) {
               cb(OtcClientDeal::error("invalid settl addr"));
               return;
            }

            const auto changedCallback = nullptr;
            const bool isSegWitInputsOnly = true;
            const bool confirmedOnly = true;
            auto transaction = std::make_shared<TransactionData>(changedCallback, logger_, isSegWitInputsOnly, confirmedOnly);

            auto resetInputsCb = [this, cb, peer, transaction, settlAddr, feePerByte, settlementId, targetHdWallet, handle]() {
               // resetInputsCb will be destroyed when returns, create one more callback to hold variables
               QMetaObject::invokeMethod(this, [this, cb, peer, transaction, settlAddr, feePerByte, settlementId, targetHdWallet, handle, logger = logger_] {
                  if (!handle.isValid()) {
                     SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
                     return;
                  }

                  const double amount = peer->offer.amount / BTCNumericTypes::BalanceDivider;

                  if (peer->offer.ourSide == bs::network::otc::Side::Sell) {
                     // Seller
                     auto index = transaction->RegisterNewRecipient();
                     assert(index == 0);
                     transaction->UpdateRecipient(0, amount, settlAddr);

                     if (!transaction->IsTransactionValid()) {
                        cb(OtcClientDeal::error("invalid pay-in transaction"));
                        return;
                     }

                     const auto cbPreimage = [cb, peer, transaction, settlAddr, settlementId, targetHdWallet, handle, amount, logger = logger_]
                        (const std::map<bs::Address, BinaryData> &preimages)
                     {
                        if (!handle.isValid()) {
                           SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
                           return;
                        }

                        const auto resolver = bs::sync::WalletsManager::getPublicResolver(preimages);

                        auto changeCb = [cb, peer, transaction, settlAddr, settlementId, targetHdWallet, handle, amount, logger, resolver]
                           (const bs::Address &changeAddr)
                        {
                           if (!handle.isValid()) {
                              SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
                              return;
                           }

                           peer->settlementId = settlementId;

                           OtcClientDeal result;
                           result.settlementId = settlementId;
                           result.settlementAddr = settlAddr;
                           result.ourAuthAddress = peer->offer.authAddress;
                           result.cpPubKey = peer->authPubKey;
                           result.amount = peer->offer.amount;
                           result.price = peer->offer.price;
                           result.hdWalletId = targetHdWallet->walletId();
                           result.success = true;
                           result.side = otc::Side::Sell;
                           result.payin = transaction->createTXRequest(false, changeAddr);
                           result.payinTxId = result.payin.txId(resolver);
                           auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, result.payinTxId, bs::XBTAmount{amount});
                           result.fee = int64_t(result.payin.fee);
                           peer->sellFromOffline = targetHdWallet->isOffline();
                           cb(std::move(result));
                        };

                        transaction->GetFallbackRecvAddress(changeCb);
                     };

                     const auto addrMapping = walletsMgr_->getAddressToWalletsMapping(transaction->inputs());
                     signContainer_->getAddressPreimage(addrMapping, cbPreimage);
                     return;
                  }

                  // Buyer

                  peer->settlementId = settlementId;

                  auto recvAddrCb = [this, cb, settlementId, settlAddr, peer, targetHdWallet, feePerByte, amount, handle, logger, transaction](const bs::Address &outputAddr) {
                     if (!handle.isValid()) {
                        SPDLOG_LOGGER_ERROR(logger, "peer was destroyed");
                        return;
                     }

                     OtcClientDeal result;
                     result.settlementId = settlementId;
                     result.settlementAddr = settlAddr;
                     result.ourAuthAddress = peer->offer.authAddress;
                     result.cpPubKey = peer->authPubKey;
                     result.amount = peer->offer.amount;
                     result.price = peer->offer.price;
                     result.hdWalletId = targetHdWallet->walletId();
                     result.success = true;
                     result.side = otc::Side::Buy;
                     auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, peer->payinTxIdFromSeller, bs::XBTAmount{ amount });
                     result.payout = bs::SettlementMonitor::createPayoutTXRequest(
                        payinUTXO, outputAddr, feePerByte, armory_->topBlock());
                     result.fee = int64_t(result.payout.fee);
                     cb(std::move(result));
                  };

                  if (peer->offer.recvAddress.empty()) {
                     transaction->GetFallbackRecvAddress(std::move(recvAddrCb));
                  } else {
                     recvAddrCb(bs::Address(peer->offer.recvAddress));
                  }
               }, Qt::QueuedConnection);
            };

            transaction->setFeePerByte(feePerByte);

            if (peer->offer.inputs.empty()) {
               transaction->setGroup(targetHdWallet->getGroup(targetHdWallet->getXBTGroupType()), armory_->topBlock(), false, resetInputsCb);
            } else {
               transaction->setGroupAndInputs(targetHdWallet->getGroup(targetHdWallet->getXBTGroupType()), peer->offer.inputs, armory_->topBlock());
               transaction->getSelectedInputs()->SetUseAutoSel(true);
               resetInputsCb();
            }
         };

         const bool myKeyFirst = (peer->offer.ourSide == bs::network::otc::Side::Buy);
         primaryHdWallet->getSettlementPayinAddress(SecureBinaryData::CreateFromHex(settlementId), peer->authPubKey, cbSettlAddr, myKeyFirst);
      };
      walletsMgr_->estimatedFeePerByte(2, cbFee, this);
   });
}

void OtcClient::sendSellerAccepts(Peer *peer)
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
   auto wallet = walletsMgr_->getPrimaryWallet();
   if (!wallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find primary wallet");
      return nullptr;
   }

   auto group = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(wallet->getGroup(bs::hd::BlockSettle_Settlement));
   if (!group) {
      SPDLOG_LOGGER_ERROR(logger_, "don't have settlement group");
      return nullptr;
   }

   return group->getLeaf(bs::Address(ourAuthAddress));
}

void OtcClient::changePeerStateWithoutUpdate(Peer *peer, State state)
{
   SPDLOG_LOGGER_DEBUG(logger_, "changing peer '{}' state from {} to {}"
      , peer->toString(), toString(peer->state), toString(state));
   peer->state = state;
   peer->stateTimestamp = std::chrono::steady_clock::now();
}

void OtcClient::changePeerState(Peer *peer, bs::network::otc::State state)
{
   changePeerStateWithoutUpdate(peer, state);
   emit peerUpdated(peer);
}

void OtcClient::resetPeerStateToIdle(Peer *peer)
{
   if (!peer->activeSettlementId.isNull()) {
      signContainer_->CancelSignTx(peer->activeSettlementId);
      peer->activeSettlementId.clear();
   }

   changePeerStateWithoutUpdate(peer, State::Idle);
   auto request = std::move(peer->request);
   *peer = Peer(peer->contactId, peer->type);
   peer->request = std::move(request);
   emit peerUpdated(peer);
}

void OtcClient::scheduleCloseAfterTimeout(std::chrono::milliseconds timeout, Peer *peer)
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

   deal->addressVerificator = std::make_unique<AddressVerificator>(logger_, armory_, [this, deal]
      (const bs::Address &address, AddressVerificationState state)
   {
      SPDLOG_LOGGER_DEBUG(logger_, "counterparty's address verification {} for {}", to_string(state), address.display());
      if (state == AddressVerificationState::Verified) {
         bs::sync::PasswordDialogData dialogData;
         dialogData.setValue(deal->isRequestor() ? PasswordDialogData::ResponderAuthAddressVerified : PasswordDialogData::RequesterAuthAddressVerified, true);
         dialogData.setValue(PasswordDialogData::SettlementId, deal->settlementId);
         signContainer_->updateDialogData(dialogData);
      }
   });

   deal->addressVerificator->SetBSAddressList(authAddressManager_->GetBSAddresses());
   // Verify only peer's auth address
   deal->addressVerificator->addAddress(deal->isRequestor() ? deal->responderAuthAddress() : deal->requestorAuthAddress());
   deal->addressVerificator->startAddressVerification();
}

void OtcClient::setComments(OtcClientDeal *deal)
{
   auto hdWallet = walletsMgr_->getHDWalletById(deal->hdWalletId);
   auto group = hdWallet ? hdWallet->getGroup(hdWallet->getXBTGroupType()) : nullptr;
   auto leaves = group ? group->getAllLeaves() : std::vector<std::shared_ptr<bs::sync::Wallet>>();
   for (const auto & leaf : leaves) {
      auto comment = fmt::format("{} XBT/EUR @ {} (OTC)"
         , bs::network::otc::toString(deal->side), UiUtils::displayPriceFX(bs::network::otc::fromCents(deal->price)).toStdString());
      leaf->setTransactionComment(deal->signedTx, comment);
   }
}

void OtcClient::updatePublicLists()
{
   contacts_.clear();
   contacts_.reserve(contactMap_.size());
   for (auto &item : contactMap_) {
      contacts_.push_back(&item.second);
   }

   requests_.clear();
   requests_.reserve(requestMap_.size() + (ownRequest_ ? 1 : 0));
   if (ownRequest_) {
      requests_.push_back(ownRequest_.get());
   }
   for (auto &item : requestMap_) {
      requests_.push_back(&item.second);
   }

   responses_.clear();
   responses_.reserve(responseMap_.size());
   for (auto &item : responseMap_) {
      responses_.push_back(&item.second);
   }

   emit publicUpdated();
}
