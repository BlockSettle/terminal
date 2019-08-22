#include "OtcClient.h"

#include <QTimer>
#include <spdlog/spdlog.h>

#include "BtcUtils.h"
#include "EncryptionUtils.h"
#include "SettlementMonitor.h"
#include "TransactionData.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "otc.pb.h"

using namespace Blocksettle::Communication::Otc;
using namespace Blocksettle::Communication;
using namespace bs::network;
using namespace bs::network::otc;

namespace {

   const int SettlementIdSize = 32;
   const int TxHashSize = 32;
   const int PubKeySize = 33;

   bs::sync::PasswordDialogData toPasswordDialogData()
   {
      bs::sync::PasswordDialogData dialogData;

//      dialogData.setValue("ProductGroup", tr(bs::network::Asset::toString(assetType())));
//      dialogData.setValue("Security", QString::fromStdString(security()));
//      dialogData.setValue("Product", QString::fromStdString(product()));
//      dialogData.setValue("Side", tr(bs::network::Side::toString(side())));

//      // rfq details
//      QString qtyProd = UiUtils::XbtCurrency;
//      QString fxProd = QString::fromStdString(fxProduct());

//      dialogData.setValue("Title", tr("Settlement Transaction"));

//      dialogData.setValue("Price", UiUtils::displayPriceXBT(price()));
//      dialogData.setValue("TransactionAmount", UiUtils::displayQuantity(amount(), UiUtils::XbtCurrency));

//      dialogData.setValue("Quantity", tr("%1 %2")
//                          .arg(UiUtils::displayAmountForProduct(amount(), qtyProd, bs::network::Asset::Type::SpotXBT))
//                          .arg(qtyProd));
//      dialogData.setValue("TotalValue", tr("%1 %2")
//                    .arg(UiUtils::displayAmountForProduct(amount() * price(), fxProd, bs::network::Asset::Type::SpotXBT))
//                    .arg(fxProd));


//      // tx details
//      if (weSell()) {
//         dialogData.setValue("TotalSpent", UiUtils::displayQuantity(amount() + UiUtils::amountToBtc(fee()), UiUtils::XbtCurrency));
//      }
//      else {
//         dialogData.setValue("TotalReceived", UiUtils::displayQuantity(amount() - UiUtils::amountToBtc(fee()), UiUtils::XbtCurrency));
//      }

//      dialogData.setValue("TransactionAmount", UiUtils::displayQuantity(amount(), UiUtils::XbtCurrency));
//      dialogData.setValue("NetworkFee", UiUtils::displayQuantity(UiUtils::amountToBtc(fee()), UiUtils::XbtCurrency));

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
}

const Peer *OtcClient::peer(const std::string &peerId) const
{
   auto it = peers_.find(peerId);
   if (it == peers_.end()) {
      return nullptr;
   }
   return &it->second;
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
   assert(ourPubKey_.getSize() == PubKeySize);

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
         break;
      case Message::kSellerOffers:
         processSellerOffers(peer, message.seller_offers());
         break;
      case Message::kBuyerAccepts:
         processBuyerAccepts(peer, message.buyer_accepts());
         break;
      case Message::kSellerAccepts:
         processSellerAccepts(peer, message.seller_accepts());
         break;
      case Message::kBuyerAcks:
         processBuyerAcks(peer, message.buyer_acks());
         break;
      case Message::kClose:
         processClose(peer, message.close());
         break;
      case Message::DATA_NOT_SET:
         blockPeer("unknown or empty OTC message", peer);
         break;
   }

   emit peerUpdated(peerId);
}

void OtcClient::processBuyerOffers(Peer *peer, const Message_BuyerOffers &msg)
{
   if (!isValidOffer(msg.offer())) {
      blockPeer("invalid offer", peer);
      return;
   }

   if (msg.auth_address_buyer().size() != PubKeySize) {
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

   if (msg.auth_address_buyer().size() != PubKeySize) {
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

   if (msg.settlement_id().size() != SettlementIdSize) {
      blockPeer("invalid settlement_id in SellerAccepts message", peer);
      return;
   }
   auto settlementId = BinaryData(msg.settlement_id());

   if (msg.auth_address_seller().size() != PubKeySize) {
      blockPeer("invalid auth_address_seller in SellerAccepts message", peer);
      return;
   }
   peer->authPubKey = msg.auth_address_seller();

   if (msg.payin_tx_id().size() != TxHashSize) {
      blockPeer("invalid payin_tx_id in SellerAccepts message", peer);
      return;
   }
   peer->payinTxIdFromSeller = BinaryData(msg.payin_tx_id());

   createRequests(settlementId, *peer, [this, settlementId, offer = peer->offer, peerId = peer->peerId]
      (SignRequestPtr payin, SignRequestPtr payoutFallback, SignRequestPtr payout)
   {
      if (!payout) {
         SPDLOG_LOGGER_ERROR(logger_, "creating pay-out sign request fails");
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

      Message msg;
      msg.mutable_buyer_acks();
      send(peer, msg);

      changePeerState(peer, State::Idle);

      // TODO: Send buyer details to PB
      SPDLOG_LOGGER_INFO(logger_, "#### success ####");
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

   // TODO: Send seller details to PB

   SPDLOG_LOGGER_INFO(logger_, "#### success ####");
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

void OtcClient::createRequests(const BinaryData &settlementId, const Peer &peer, const SignRequestsCb &cb)
{
   assert(peer.authPubKey.getSize() == PubKeySize);
   assert(settlementId.getSize() == SettlementIdSize);

   if (peer.offer.ourSide == bs::network::otc::Side::Buy) {
      assert(peer.payinTxIdFromSeller.getSize() == TxHashSize);
   }

   auto leaf = ourSettlementLeaf();
   if (!leaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf");
      cb(nullptr, nullptr, nullptr);
      return;
   }


   leaf->setSettlementID(settlementId, [this, settlementId, peer, cb](bool result) {
      if (!result) {
         SPDLOG_LOGGER_ERROR(logger_, "setSettlementID failed");
         cb(nullptr, nullptr, nullptr);
         return;
      }

      auto cbFee = [this, cb, peer, settlementId](float feePerByte) {
         if (feePerByte < 1) {
            SPDLOG_LOGGER_ERROR(logger_, "invalid fees detected");
            cb(nullptr, nullptr, nullptr);
            return;
         }

         auto hdWallet = walletsMgr_->getPrimaryWallet();
         if (!hdWallet) {
            SPDLOG_LOGGER_ERROR(logger_, "can't find primary wallet");
            cb(nullptr, nullptr, nullptr);
            return;
         }

         auto cbSettlAddr = [this, cb, peer, feePerByte, settlementId](const bs::Address &settlAddr) {
            if (settlAddr.isNull()) {
               SPDLOG_LOGGER_ERROR(logger_, "invalid settl addr");
               cb(nullptr, nullptr, nullptr);
               return;
            }

            auto wallet = ourBtcWallet();
            if (!wallet) {
               SPDLOG_LOGGER_ERROR(logger_, "can't find BTC wallet");
               cb(nullptr, nullptr, nullptr);
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

                  if (peer.offer.ourSide == bs::network::otc::Side::Sell) {
                     // Seller
                     auto index = transaction->RegisterNewRecipient();
                     assert(index == 0);
                     transaction->UpdateRecipient(0, amount, settlAddr);

                     if (!transaction->IsTransactionValid()) {
                        SPDLOG_LOGGER_ERROR(logger_, "invalid pay-in transaction");
                        cb(nullptr, nullptr, nullptr);
                        return;
                     }

                     auto payinTx = std::make_unique<bs::core::wallet::TXSignRequest>(transaction->createTXRequest());
                     auto payinTxId = payinTx->txId();
                     auto fallbackAddr = transaction->GetFallbackRecvAddress();
                     auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, payinTxId, amount);
                     auto payoutTxFallback = std::make_unique<bs::core::wallet::TXSignRequest>(bs::SettlementMonitor::createPayoutTXRequest(
                        payinUTXO, fallbackAddr, feePerByte, armory_->topBlock()));

                     cb(std::move(payinTx), std::move(payoutTxFallback), nullptr);
                     return;
                  }

                  // Buyer
                  auto outputAddr = transaction->GetFallbackRecvAddress();
                  auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, peer.payinTxIdFromSeller, amount);
                  auto payoutTx = std::make_unique<bs::core::wallet::TXSignRequest>(bs::SettlementMonitor::createPayoutTXRequest(
                     payinUTXO, outputAddr, feePerByte, armory_->topBlock()));

                  cb(nullptr, nullptr, std::move(payoutTx));
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
   assert(ourPubKey_.getSize() == PubKeySize);
   auto settlementId = CryptoPRNG::generateRandom(SettlementIdSize);

   createRequests(settlementId, *peer, [this, settlementId, offer = peer->offer, peerId = peer->peerId]
      (SignRequestPtr payin, SignRequestPtr payoutFallback, SignRequestPtr payout)
   {
      if (!payin || !payoutFallback) {
         SPDLOG_LOGGER_ERROR(logger_, "creating pay-in sign request fails");
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

      auto payinTxId = payin->txId();

      Message msg;
      auto d = msg.mutable_seller_accepts();
      copyOffer(peer->offer, d->mutable_offer());
      d->set_settlement_id(settlementId.toBinStr());
      d->set_auth_address_seller(ourPubKey_.toBinStr());
      d->set_payin_tx_id(payinTxId.toBinStr());
      send(peer, msg);

      changePeerState(peer, State::SentPayinInfo);
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
