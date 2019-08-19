#include "OtcClient.h"

#include <QTimer>
#include <spdlog/spdlog.h>

#include "BtcUtils.h"
#include "EncryptionUtils.h"
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

   const int RandomKeySize = 32;
   const int PubKeySize = 33;

   bs::sync::PasswordDialogData toPasswordDialogData()
   {
      bs::sync::PasswordDialogData dialogData;

      dialogData.setValue("ProductGroup", QObject::tr("qqq"));

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

   auto leaf = ourSettlementLeaf();
   if (!leaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf");
      return false;
   }

   leaf->getRootPubkey([this, peerId, offer](const SecureBinaryData &pubKey) {
      if (pubKey.isNull()) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid pubKey");
         return;
      }

      auto peer = findPeer(peerId);
      if (!peer) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
         return;
      }

      if (peer->state != State::Idle) {
         SPDLOG_LOGGER_ERROR(logger_, "can't send offer to '{}', peer should be in idle state", peerId);
         return;
      }

      peer->random_part1 = CryptoPRNG::generateRandom(RandomKeySize);

      Message msg;
      auto d = msg.mutable_offer();
      d->set_price(offer.price);
      d->set_amount(offer.amount);
      d->set_sender_side(Otc::Side(offer.ourSide));
      d->set_random_part1(peer->random_part1.toBinStr());
      d->set_auth_address(pubKey.toBinStr());
      send(peer, msg);

      peer->state = State::OfferSent;
      peer->offer = offer;
      emit peerUpdated(peerId);
   });

   return true;
}

bool OtcClient::pullOrRejectOffer(const std::string &peerId)
{
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

   peer->state = State::Idle;
   emit peerUpdated(peerId);
   return true;
}

bool OtcClient::acceptOffer(const bs::network::otc::Offer &offer, const std::string &peerId)
{
   assert(offer.ourSide != otc::Side::Unknown);
   assert(offer.amount > 0);
   assert(offer.price > 0);

   auto peer = findPeer(peerId);
   if (!peer) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
      return false;
   }

   if (peer->state != State::OfferRecv) {
      SPDLOG_LOGGER_ERROR(logger_, "can't accept offer from '{}', we should be in OfferRecv state", peerId);
      return false;
   }

   peer->random_part2 = CryptoPRNG::generateRandom(RandomKeySize);

   auto leaf = ourSettlementLeaf();
   if (!leaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf");
      return false;
   }

   leaf->getRootPubkey([this, peerId, offer](const SecureBinaryData &ourPubKey) {
      auto peer = findPeer(peerId);
      if (!peer) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
         return;
      }

      if (peer->state != State::OfferRecv) {
         SPDLOG_LOGGER_ERROR(logger_, "can't accept offer from '{}', we should be in OfferRecv state", peerId);
         return;
      }

      if (ourPubKey.isNull()) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid pubKey");
         return;
      }

      auto leaf = ourSettlementLeaf();
      if (!leaf) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf");
         return;
      }

      auto settlementId = getSettlementId(*peer);
      assert(!settlementId.isNull());

      leaf->setSettlementID(settlementId, [this, ourPubKey, settlementId, offer, peerId](bool result) {
         if (!result) {
            SPDLOG_LOGGER_ERROR(logger_, "setSettlementID failed");
            return;
         }

         const auto &cbFee = [this, peerId, offer, ourPubKey, settlementId](float feePerByte) {
            if (feePerByte < 1) {
               SPDLOG_LOGGER_ERROR(logger_, "invalid fees detected");
               return;
            }

            auto hdWallet = walletsMgr_->getPrimaryWallet();
            if (!hdWallet) {
               SPDLOG_LOGGER_ERROR(logger_, "can't find primary wallet");
               return;
            }

            auto peer = findPeer(peerId);
            if (!peer) {
               SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
               return;
            }

            if (peer->state != State::OfferRecv) {
               SPDLOG_LOGGER_ERROR(logger_, "can't accept offer from '{}', we should be in OfferRecv state", peerId);
               return;
            }

            auto cbSettlAddr = [this, offer, peerId, ourPubKey, feePerByte](const bs::Address &settlAddr) {
               if (settlAddr.isNull()) {
                  SPDLOG_LOGGER_ERROR(logger_, "invalid settl addr");
                  return;
               }

               auto peer = findPeer(peerId);
               if (!peer) {
                  SPDLOG_LOGGER_ERROR(logger_, "can't find peer '{}'", peerId);
                  return;
               }

               if (peer->state != State::OfferRecv) {
                  SPDLOG_LOGGER_ERROR(logger_, "can't accept offer from '{}', we should be in OfferRecv state", peerId);
                  return;
               }

               auto wallet = ourBtcWallet();
               if (!wallet) {
                  SPDLOG_LOGGER_ERROR(logger_, "can't find BTC wallet");
                  return;
               }

               Message msg;
               auto d = msg.mutable_accept();
               d->set_price(offer.price);
               d->set_amount(offer.amount);
               d->set_sender_side(Otc::Side(offer.ourSide));
               d->set_random_part2(peer->random_part2.toBinStr());
               d->set_auth_address(ourPubKey.toBinStr());
               send(peer, msg);

               // Add timeout detection here
               peer->state = State::WaitAcceptAck;
               emit peerUpdated(peerId);

               assert(peer->authPubKey.getSize() == PubKeySize);

               auto transaction = std::make_shared<TransactionData>([]() {}, nullptr, true, true);
               transaction->setWallet(wallet, armory_->topBlock());
               transaction->setFeePerByte(feePerByte);

               auto index = transaction->RegisterNewRecipient();
               assert(index == 0);
               transaction->UpdateRecipient(0, offer.amount / BTCNumericTypes::BalanceDivider, settlAddr);
               QTimer::singleShot(std::chrono::seconds(1), [this, transaction] {
                  auto txRequest = transaction->createTXRequest();
                  auto reqId = signContainer_->signSettlementTXRequest(txRequest, toPasswordDialogData());

                  connect(signContainer_.get(), &SignContainer::TXSigned, this, [this, reqId](bs::signer::RequestId id, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason) {
                     if (reqId != id) {
                        return;
                     }
                     if (result != bs::error::ErrorCode::NoError) {
                        return;
                     }
                     armory_->broadcastZC(signedTX);
                  });

                  return;
               });

               SPDLOG_LOGGER_DEBUG(logger_, "#### success: {}", settlAddr.display());
            };

            const bool myKeyFirst = (peer->offer.ourSide == bs::network::otc::Side::Buy);
            hdWallet->getSettlementPayinAddress(settlementId, peer->authPubKey, cbSettlAddr, myKeyFirst);

            SPDLOG_LOGGER_DEBUG(logger_, "success");
         };
         walletsMgr_->estimatedFeePerByte(2, cbFee, this);
      });

   });

   return true;
}

bool OtcClient::updateOffer(const Offer &offer, const std::string &peerId)
{
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

   assert(offer.amount == peer->offer.amount);
   assert(offer.ourSide == peer->offer.ourSide);

   peer->random_part1 = CryptoPRNG::generateRandom(RandomKeySize);

   Message msg;
   auto d = msg.mutable_offer();
   d->set_price(offer.price);
   d->set_amount(offer.amount);
   d->set_sender_side(Otc::Side(offer.ourSide));
   d->set_random_part1(peer->random_part1.toBinStr());
   send(peer, msg);

   peer->state = State::OfferSent;
   peer->offer = offer;
   emit peerUpdated(peerId);
   return true;
}

// static
BinaryData OtcClient::getSettlementId(const Peer &peer)
{
   if (peer.random_part1.getSize() != RandomKeySize || peer.random_part2.getSize() != RandomKeySize) {
      return BinaryData();
   }

   return BtcUtils::getSha256(peer.random_part1 + peer.random_part2);
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
      case Message::kOffer:
         processOffer(peer, message.offer());
         break;
      case Message::kAccept:
         processAccept(peer, message.accept());
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

void OtcClient::processOffer(Peer *peer, const Message_Offer &msg)
{
   if (!isValidSide(otc::Side(msg.sender_side()))) {
      blockPeer("invalid offer side", peer);
      return;
   }

   if (msg.amount() <= 0) {
      blockPeer("invalid offer amount", peer);
      return;
   }

   if (msg.price() <= 0) {
      blockPeer("invalid offer price", peer);
      return;
   }

   if (msg.random_part1().size() != RandomKeySize) {
      blockPeer("invalid random_part1", peer);
      return;
   }

   if (msg.auth_address().size() != PubKeySize) {
      blockPeer("invalid auth_address in offer", peer);
      return;
   }

   switch (peer->state) {
      case State::Idle:
         peer->state = State::OfferRecv;
         peer->offer.ourSide = switchSide(otc::Side(msg.sender_side()));
         peer->offer.amount = msg.amount();
         peer->offer.price = msg.price();
         peer->random_part1 = msg.random_part1();
         peer->authPubKey = msg.auth_address();
         emit peerUpdated(peer->peerId);
         break;

      case State::OfferSent:
         if (peer->offer.ourSide != switchSide(otc::Side(msg.sender_side()))) {
            blockPeer("unexpected side in counter-offer", peer);
            return;
         }
         if (peer->offer.amount != msg.amount()) {
            blockPeer("invalid amount in counter-offer", peer);
            return;
         }

         peer->state = State::OfferRecv;
         peer->offer.price = msg.price();
         peer->random_part1 = msg.random_part1();
         emit peerUpdated(peer->peerId);
         break;

      case State::OfferRecv:
      case State::WaitAcceptAck:
         blockPeer("unexpected offer", peer);
         break;

      case State::Blacklisted:
         assert(false);
         break;
   }
}

void OtcClient::processAccept(Peer *peer, const Message_Accept &msg)
{
   switch (peer->state) {
      case State::OfferSent: {
         if (msg.random_part2().size() != RandomKeySize) {
            blockPeer("invalid random_part2", peer);
            return;
         }

         if (otc::Side(msg.sender_side()) != switchSide(peer->offer.ourSide)) {
            blockPeer("unexpected accepted sender side", peer);
            return;
         }

         if (msg.price() != peer->offer.price || msg.amount() != peer->offer.amount) {
            blockPeer("unexpected accepted price or amount", peer);
            return;
         }

         if (msg.auth_address().size() != PubKeySize) {
            blockPeer("invalid auth_address in accept", peer);
            return;
         }

         peer->random_part2 = msg.random_part2();

         Message reply;
         auto d = reply.mutable_accept();
         d->set_sender_side(Otc::Side(peer->offer.ourSide));
         d->set_amount(peer->offer.amount);
         d->set_price(peer->offer.price);
         send(peer, reply);

         // TODO: Send details to PB
         *peer = Peer(peer->peerId);
         break;
      }

      case State::WaitAcceptAck: {
         auto senderSide = otc::Side(msg.sender_side());
         auto expectedSenderSide = switchSide(otc::Side(peer->offer.ourSide));
         if (senderSide != expectedSenderSide) {
            blockPeer(fmt::format("unexpected accepted sender side: {}, expected: {}"
               , toString(senderSide), toString(expectedSenderSide)), peer);
            return;
         }

         if (msg.price() != peer->offer.price || msg.amount() != peer->offer.amount) {
            blockPeer("unexpected accepted price or amount", peer);
            return;
         }

         // TODO: Send details to PB
         *peer = Peer(peer->peerId);
         emit peerUpdated(peer->peerId);
         break;
      }

      case State::Idle:
      case State::OfferRecv:
         blockPeer("unexpected accept", peer);
         break;

      case State::Blacklisted:
         assert(false);
         break;
   }
}

void OtcClient::processClose(Peer *peer, const Message_Close &msg)
{
   switch (peer->state) {
      case State::OfferSent:
      case State::OfferRecv:
         *peer = Peer(peer->peerId);
         break;

      case State::WaitAcceptAck:
      case State::Idle:
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
