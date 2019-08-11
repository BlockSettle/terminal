#ifndef OTC_TYPES_H
#define OTC_TYPES_H

#include <cstdint>
#include <string>

namespace bs {
   namespace network {
      namespace otc {

         enum class State
         {
            // No data received
            Idle,

            // We sent offer
            OfferSent,

            // We recv offer
            OfferRecv,

            // We have accepted recv offer and wait for ack.
            // This should happen without user's intervention.
            WaitAcceptAck,

            // Peer does not comply to protocol, block it
            Blacklisted,
         };

         std::string toString(State state);

         // Keep in sync with Chat.OtcSide
         enum class Side {
            Unknown,
            Buy,
            Sell
         };

         std::string toString(Side side);
         bool isValidSide(Side side);

         Side switchSide(Side side);

         struct Range
         {
            int64_t lower;
            int64_t upper;
         };

         // Keep in sync with Chat.OtcRangeType
         enum class RangeType
         {
            Range1_5,
            Range5_10,
            Range10_50,
            Range50_100,
            Range100_250,
            Range250plus,

            Count
         };

         std::string toString(RangeType range);

         Range getRange(RangeType range);

         struct Offer
         {
            Side ourSide{};
            int64_t amount{};
            int64_t price{};

            bool operator==(const Offer &other) const;
            bool operator!=(const Offer &other) const;
         };

         struct Peer
         {
            std::string peerId;
            bs::network::otc::Offer offer;
            bs::network::otc::State state{bs::network::otc::State::Idle};

            Peer(const std::string &peerId)
               : peerId(peerId)
            {
            }
         };
      }
   }
}

#endif
