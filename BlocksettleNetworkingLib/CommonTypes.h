#ifndef __BS_COMMON_TYPES_H__
#define __BS_COMMON_TYPES_H__

#include <QObject>
#include <QDateTime>
#include <QString>
#include "Address.h"
#include "com/celertech/marketmerchant/api/enums/SideProto.pb.h"
#include "com/celertech/marketmerchant/api/enums/AssetTypeProto.pb.h"
#include "com/celertech/marketmerchant/api/enums/ProductTypeProto.pb.h"
#include "com/celertech/marketmerchant/api/enums/MarketDataEntryTypeProto.pb.h"

#include "com/celertech/marketdata/api/enums/AssetTypeProto.pb.h"
#include "com/celertech/marketdata/api/enums/ProductTypeProto.pb.h"
#include "com/celertech/marketdata/api/enums/MarketDataEntryTypeProto.pb.h"

#ifndef NDEBUG
#include <stdexcept>
#endif

namespace bs {
   namespace network {

      struct Side {
         enum Type {
            Undefined,
            Buy,
            Sell
         };

         static Type fromCeler(com::celertech::marketmerchant::api::enums::side::Side side) {
            switch (side) {
               case com::celertech::marketmerchant::api::enums::side::BUY:    return Buy;
               case com::celertech::marketmerchant::api::enums::side::SELL:   return Sell;
            }
            return Undefined;
         }
         static com::celertech::marketmerchant::api::enums::side::Side toCeler(Type side) {
            switch (side) {
               case Buy:   return com::celertech::marketmerchant::api::enums::side::BUY;
               case Sell:
               default:    return com::celertech::marketmerchant::api::enums::side::SELL;
            }
         }
         static const char *toString(Type side) {
            switch (side) {
               case Buy:   return QT_TR_NOOP("BUY");
               case Sell:  return QT_TR_NOOP("SELL");
               default:    return "unknown";
            }
         }
         static const char *responseToString(Type side) {
            switch (side) {
            case Buy:   return QT_TR_NOOP("Offer");
            case Sell:  return QT_TR_NOOP("Bid");
            default:    return "";
            }
         }
         static Type invert(Type side) {
            switch (side) {
               case Buy:   return Sell;
               case Sell:  return Buy;
               default:    return side;
            }
         }
      };


      struct Asset {
         enum Type {
            Undefined,
            first,
            SpotFX = first,
            SpotXBT,
            PrivateMarket,
            last
         };

         static Type fromCelerProductType(com::celertech::marketdata::api::enums::producttype::ProductType pt) {
            switch (pt) {
            case com::celertech::marketdata::api::enums::producttype::SPOT:           return SpotFX;
            case com::celertech::marketdata::api::enums::producttype::BITCOIN:        return SpotXBT;
            case com::celertech::marketdata::api::enums::producttype::PRIVATE_SHARE:  return PrivateMarket;
            default: return Undefined;
            }
         }

         static Type fromCelerProductType(com::celertech::marketmerchant::api::enums::producttype::ProductType pt) {
            switch (pt) {
            case com::celertech::marketmerchant::api::enums::producttype::SPOT:           return SpotFX;
            case com::celertech::marketmerchant::api::enums::producttype::BITCOIN:        return SpotXBT;
            case com::celertech::marketmerchant::api::enums::producttype::PRIVATE_SHARE:  return PrivateMarket;
            default: return Undefined;
            }
         }
         static com::celertech::marketmerchant::api::enums::assettype::AssetType toCeler(Type at) {
            switch (at) {
               case SpotFX:         return com::celertech::marketmerchant::api::enums::assettype::FX;
               case SpotXBT:        return com::celertech::marketmerchant::api::enums::assettype::CRYPTO;
               case PrivateMarket:  return com::celertech::marketmerchant::api::enums::assettype::CRYPTO;
               default:             return com::celertech::marketmerchant::api::enums::assettype::STRUCTURED_PRODUCT;
            }
         }
         static com::celertech::marketdata::api::enums::assettype::AssetType toCelerMDAssetType(Type at) {
            switch (at) {
               case SpotFX:         return com::celertech::marketdata::api::enums::assettype::FX;
               case SpotXBT:        // fall through
               case PrivateMarket:  // fall through
               default:
                                    return com::celertech::marketdata::api::enums::assettype::CRYPTO;
            }
         }
         static com::celertech::marketmerchant::api::enums::producttype::ProductType toCelerProductType(Type at) {
            switch (at) {
               case SpotFX:         return com::celertech::marketmerchant::api::enums::producttype::SPOT;
               case SpotXBT:        return com::celertech::marketmerchant::api::enums::producttype::BITCOIN;
               case PrivateMarket:  return com::celertech::marketmerchant::api::enums::producttype::PRIVATE_SHARE;
               default:             return com::celertech::marketmerchant::api::enums::producttype::SPOT;
            }
         }
         static com::celertech::marketdata::api::enums::producttype::ProductType toCelerMDProductType(Type at) {
            switch (at) {
               case SpotFX:         return com::celertech::marketdata::api::enums::producttype::SPOT;
               case SpotXBT:        return com::celertech::marketdata::api::enums::producttype::BITCOIN;
               case PrivateMarket:  return com::celertech::marketdata::api::enums::producttype::PRIVATE_SHARE;
               default:             return com::celertech::marketdata::api::enums::producttype::SPOT;
            }
         }
         static const char *toCelerSettlementType(Type at) {
            switch (at) {
            case SpotFX:         return "SPOT";
            case SpotXBT:        return "XBT";
            case PrivateMarket:  return "CC";
            default:       return "";
            }
         }
         static const char *toString(Type at) {
            switch (at) {
            case SpotFX:   return QT_TR_NOOP("Spot FX");
            case SpotXBT:  return QT_TR_NOOP("Spot XBT");
            case PrivateMarket:  return QT_TR_NOOP("Private Market");
            default:       return "";
            }
         }
      };


      struct RFQ
      {
         std::string requestId;
         std::string security;
         std::string product;

         Asset::Type assetType;
         Side::Type  side;

         double quantity;
         // requestorAuthPublicKey - public key HEX encoded
         std::string requestorAuthPublicKey;
         std::string receiptAddress;
         std::string coinTxInput;

         bool isXbtBuy() const;
      };


      struct Quote
      {
         double price;
         double quantity;
         std::string requestId;
         std::string quoteId;
         std::string security;
         std::string product;

         // requestorAuthPublicKey - public key HEX encoded
         std::string requestorAuthPublicKey;
         // dealerAuthPublicKey - public key HEX encoded
         std::string dealerAuthPublicKey;
         // settlementId - HEX encoded
         std::string settlementId;
         std::string dealerTransaction;

         Side::Type  side;
         Asset::Type assetType;

         enum QuotingType {
            Automatic,
            Manual,
            Direct,
            Indicative,
            Tradeable
         };
         QuotingType quotingType;

         QDateTime   expirationTime;
         int         timeSkewMs;
         uint64_t    celerTimestamp;
      };


      struct Order
      {
         std::string clOrderId;
         QString exchOrderId;
         std::string quoteId;

         QDateTime dateTime;
         std::string security;
         std::string product;
         std::string settlementId;
         std::string reqTransaction;
         std::string dealerTransaction;
         std::string pendingStatus;
         double quantity{};
         double leavesQty{};
         double price{};
         double avgPx{};

         Side::Type  side{};
         Asset::Type assetType{};

         enum Status {
            New,
            Pending,
            Failed,
            Filled
         };
         Status status{};
      };


      struct SecurityDef {
         Asset::Type assetType;
      };


      struct QuoteReqNotification
      {
         double quantity;
         std::string quoteRequestId;
         std::string security;
         std::string product;
         std::string requestorAuthPublicKey;
         std::string sessionToken;
         std::string party;
         std::string reason;
         std::string account;
         std::string settlementId;
         std::string requestorRecvAddress;

         Side::Type  side;
         Asset::Type assetType;

         enum Status {
            StatusUndefined,
            Withdrawn,
            PendingAck,
            Replied,
            Rejected,
            TimedOut
         };
         Status status;

         QDateTime   expirationTime;
         int         timeSkewMs;
         uint64_t    celerTimestamp;

         bool empty() const { return quoteRequestId.empty(); }
      };


      struct QuoteNotification
      {
         std::string authKey;
         std::string reqAuthKey;
         std::string settlementId;
         std::string sessionToken;
         std::string quoteRequestId;
         std::string security;
         std::string product;
         std::string account;
         std::string transactionData;
         std::string receiptAddress;
         Asset::Type assetType;
         Side::Type  side;
         int         validityInS{};

         double      bidPx{};
         double      bidSz{};
         double      bidFwdPts{};
         double      bidContraQty{};

         double      offerPx{};
         double      offerSz{};
         double      offerFwdPts{};
         double      offerContraQty{};

         QuoteNotification() : validityInS(120) {}
         QuoteNotification(const bs::network::QuoteReqNotification &qrn, const std::string &_authKey, double price
            , const std::string &txData);
      };


      struct MDField;
      using MDFields = std::vector<MDField>;

      struct MDField
      {
         enum Type {
            Unknown,
            PriceBid,
            PriceOffer,
            PriceMid,
            PriceOpen,
            PriceClose,
            PriceHigh,
            PriceLow,
            PriceSettlement,
            TurnOverQty,
            VWAP,
            PriceLast,
            PriceBestBid,
            PriceBestOffer,
            DailyVolume,
            Reject,
            MDTimestamp
         };
         Type     type;
         double   value;
         QString  desc;

         static Type fromCeler(com::celertech::marketdata::api::enums::marketdataentrytype::MarketDataEntryType mdType);

         static MDField get(const MDFields &fields, Type type);
      };


      struct CCSecurityDef
      {
         std::string    securityId;
         std::string    product;
         std::string    description;
         bs::Address    genesisAddr;
         uint64_t       nbSatoshis;
      };


      const std::string XbtCurrency = "XBT";

      // fx and xbt
      struct NewTrade
      {
         std::string product;
         double      price;
         double      amount;
         uint64_t    timestamp;
      };

      struct NewPMTrade
      {
         double      price;
         uint64_t    amount;
         std::string product;
         uint64_t    timestamp;
      };

      enum class Subsystem : int
      {
         Celer = 0,
         Otc = 1,

         First = Celer,
         Last = Otc,
      };

   }  //namespace network
}  //namespace bs


Q_DECLARE_METATYPE(bs::network::Asset::Type)
Q_DECLARE_METATYPE(bs::network::Quote)
Q_DECLARE_METATYPE(bs::network::Order)
Q_DECLARE_METATYPE(bs::network::SecurityDef)
Q_DECLARE_METATYPE(bs::network::QuoteReqNotification)
Q_DECLARE_METATYPE(bs::network::QuoteNotification)
Q_DECLARE_METATYPE(bs::network::MDField)
Q_DECLARE_METATYPE(bs::network::MDFields)
Q_DECLARE_METATYPE(bs::network::CCSecurityDef)
Q_DECLARE_METATYPE(bs::network::NewTrade)
Q_DECLARE_METATYPE(bs::network::NewPMTrade)


#endif //__BS_COMMON_TYPES_H__
