#ifndef OTC_TYPES_H
#define OTC_TYPES_H

#include <chrono>
#include <cstdint>
#include <string>
#include <QDateTime>

#include "BinaryData.h"
#include "TxClasses.h"
#include "ValidityFlag.h"

namespace bs {
   namespace network {
      namespace otc {

         enum class Env : int
         {
            Prod,
            Test,
         };

         enum class PeerErrorType {
            NoError,
            Timeout,
            Canceled,
            Rejected
         };

         enum class PeerType : int
         {
            Contact,
            Request,
            Response,
         };

         std::string toString(PeerType peerType);

         enum class State
         {
            // No data received
            Idle,

            // Quote response was sent
            QuoteSent,

            // Quote response was received
            QuoteRecv,

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

            // VerifyOtc request was sent
            WaitVerification,

            WaitBuyerSign,

            WaitSellerSeal,

            WaitSellerSign,

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

         bool isSubRange(Range range, Range subRange);

         // Keep in sync with Chat.OtcRangeType
         enum class RangeType
         {
            Range0_1,
            Range1_5,
            Range5_10,
            Range10_50,
            Range50_100,
            Range100_250,
            Range250plus,
         };

         RangeType firstRangeValue(Env env);
         RangeType lastRangeValue(Env env);

         std::string toString(RangeType range);

         Range getRange(RangeType range);

         struct QuoteRequest
         {
            Side ourSide{};
            RangeType rangeType{};
            QDateTime timestamp;
         };

         struct QuoteResponse
         {
            Side ourSide{};
            Range amount{};
            Range price{};
         };

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
            std::string contactId;
            PeerType type;
            bool isOwnRequest{false};
            // Will be true for both p2p and public requesters.
            // Please note that updating price won't change this (through in such case responder would be waiting for the requester)
            bool isOurSideSentOffer{false};

            State state{State::Idle};
            // timestamp of the latest status change (it's always valid)
            std::chrono::steady_clock::time_point stateTimestamp{};

            QuoteRequest request;
            QuoteResponse response;
            Offer offer;

            BinaryData payinTxIdFromSeller;
            BinaryData authPubKey;
            BinaryData ourAuthPubKey;

            std::string settlementId;

            ValidityFlag validityFlag;

            BinaryData activeSettlementId;

            Peer(const std::string &contactId, PeerType type);

            std::string toString() const;
         };

         using Peers = std::vector<Peer*>;

         double satToBtc(int64_t value);
         double satToBtc(uint64_t value);
         int64_t btcToSat(double value);

         double fromCents(int64_t value);
         int64_t toCents(double value);

         std::chrono::milliseconds payoutTimeout();
         std::chrono::milliseconds payinTimeout();
         std::chrono::milliseconds negotiationTimeout();
         std::chrono::milliseconds publicRequestTimeout();
      }
   }
}

#endif
