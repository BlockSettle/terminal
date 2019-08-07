#include "OtcClient.h"

#include <spdlog/spdlog.h>
#include "otc.pb.h"

using namespace Blocksettle::Communication::Otc;
using namespace Blocksettle::Communication;
using namespace bs::network;
using namespace bs::network::otc;

OtcClient::OtcClient(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject (parent)
   , logger_(logger)
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

   Message msg;
   auto d = msg.mutable_offer();
   d->set_price(offer.price);
   d->set_amount(offer.amount);
   d->set_sender_side(Otc::Side(offer.ourSide));
   send(peer, msg);

   peer->state = State::OfferSent;
   peer->offer = offer;
   emit peerUpdated(peerId);
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
      SPDLOG_LOGGER_ERROR(logger_, "can't pull offer from '{}', we should be in OfferRecv state", peerId);
      return false;
   }

   Message msg;
   auto d = msg.mutable_accept();
   d->set_price(offer.price);
   d->set_amount(offer.amount);
   d->set_sender_side(Otc::Side(offer.ourSide));
   send(peer, msg);

   // Add timeout detection here
   peer->state = State::WaitAcceptAck;
   emit peerUpdated(peerId);
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

   Message msg;
   auto d = msg.mutable_offer();
   d->set_price(offer.price);
   d->set_amount(offer.amount);
   d->set_sender_side(Otc::Side(offer.ourSide));
   send(peer, msg);

   peer->state = State::OfferSent;
   peer->offer = offer;
   emit peerUpdated(peerId);
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

   switch (peer->state) {
      case State::Idle:
         peer->state = State::OfferRecv;
         peer->offer.ourSide = switchSide(otc::Side(msg.sender_side()));
         peer->offer.amount = msg.amount();
         peer->offer.price = msg.price();
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
         if (otc::Side(msg.sender_side()) != switchSide(peer->offer.ourSide)) {
            blockPeer("unexpected accepted sender side", peer);
            return;
         }

         if (msg.price() != peer->offer.price || msg.amount() != peer->offer.amount) {
            blockPeer("unexpected accepted price or amount", peer);
            return;
         }

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
