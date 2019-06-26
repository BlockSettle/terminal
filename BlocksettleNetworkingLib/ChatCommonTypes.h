#ifndef CHATCOMMONTYPES_H
#define CHATCOMMONTYPES_H

#include <inttypes.h>
#include <string>

#include <QString>

namespace bs {
   namespace network {

      struct ChatOTCSide {
         // Keep in sync with Chat.OtcSide
         enum Type {
            Undefined,
            Buy,
            Sell
         };

         static std::string toString(Type side) {
            switch (side) {
               case Buy:   return "BUY";
               case Sell:  return "SELL";
               default:    return "unknown";
            }
         }

      };

      struct OTCPriceRange
      {
         uint64_t lower;
         uint64_t upper;
      };

      struct OTCQuantityRange
      {
         uint64_t lower;
         uint64_t upper;
      };

      struct OTCRangeID
      {
         // Keep in sync with Chat.OtcRangeType
         enum class Type : int
         {
            Range1_5,
            Range5_10,
            Range10_50,
            Range50_100,
            Range100_250,
            Range250plus
         };

         static std::string toString(const Type& range)
         {
            switch(range) {
            case Type::Range1_5:
               return "1-5";
            case Type::Range5_10:
               return "5-10";
            case Type::Range10_50:
               return "10-50";
            case Type::Range50_100:
               return "50-100";
            case Type::Range100_250:
               return "100-250";
            case Type::Range250plus:
               return "250+";
            default:
#ifndef NDEBUG
               throw std::runtime_error("invalid range type");
#endif
               return "invalid range";

            }
         }

         static OTCQuantityRange toQuantityRange(const Type& range)
         {
            switch(range) {
            case Type::Range1_5:
               return OTCQuantityRange{1, 5};
            case Type::Range5_10:
               return OTCQuantityRange{5, 10};
            case Type::Range10_50:
               return OTCQuantityRange{10,50};
            case Type::Range50_100:
               return OTCQuantityRange{50, 100};
            case Type::Range100_250:
               return OTCQuantityRange{100, 250};
            case Type::Range250plus:
               return OTCQuantityRange{250, 1000};
            default:
#ifndef NDEBUG
               throw std::runtime_error("invalid range type");
#endif
               return OTCQuantityRange{1, 1};
            }
         }
      };

      struct OTCRequest
      {
         ChatOTCSide::Type    side;
         OTCRangeID::Type     amountRange;
      };

      struct OTCResponse
      {
         ChatOTCSide::Type    side;
         OTCPriceRange        priceRange;
         OTCQuantityRange     quantityRange;
      };

      struct OTCUpdate
      {
         uint64_t amount;
         uint64_t price;
      };
   }
}

#endif // CHATCOMMONTYPES_H
