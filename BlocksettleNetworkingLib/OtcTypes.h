#ifndef OTC_TYPES_H
#define OTC_TYPES_H

#include <cstdint>
#include <string>

#include "BinaryData.h"
#include "TxClasses.h"


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

            // Buy offer was accepted, wait for pay-in details
            WaitPayinInfo,

            // Sell offer was accepted and required details have been sent.
            // Wait for confirmation from peer now.
            // Payin TX will be signed after confirmation from PB.
            SentPayinInfo,

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

            std::string hdWalletId;
            std::string authAddress;

            // If set, selected inputs would be used (used for sell only)
            std::vector<UTXO> inputs;

            // If set, selected address would be used to receive XBT balance (used for buy only)
            std::string recvAddress;

            bool operator==(const Offer &other) const;
            bool operator!=(const Offer &other) const;
         };

         struct Peer
         {
            std::string peerId;
            bs::network::otc::Offer offer;
            bs::network::otc::State state{bs::network::otc::State::Idle};
            BinaryData payinTxIdFromSeller;
            BinaryData authPubKey;
            BinaryData ourAuthPubKey;

            Peer(const std::string &peerId)
               : peerId(peerId)
            {
            }
         };

         double satToBtc(int64_t value);
         double satToBtc(uint64_t value);
         int64_t btcToSat(double value);

         double fromCents(int64_t value);
         int64_t toCents(double value);
      }
   }
}

#endif
