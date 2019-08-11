#include "OtcTypes.h"

#include <cassert>

std::string bs::network::otc::toString(bs::network::otc::Side side)
{
   switch (side) {
      case Side::Buy:   return "BUY";
      case Side::Sell:  return "SELL";
      default:          return "Unknown";
   }
}

std::string bs::network::otc::toString(bs::network::otc::RangeType range)
{
   switch(range) {
      case RangeType::Range1_5:
         return "1-5";
      case RangeType::Range5_10:
         return "5-10";
      case RangeType::Range10_50:
         return "10-50";
      case RangeType::Range50_100:
         return "50-100";
      case RangeType::Range100_250:
         return "100-250";
      case RangeType::Range250plus:
         return "250+";
      case RangeType::Count:
         break;
   }

   assert(false);
   return "invalid range";
}

bs::network::otc::Range bs::network::otc::getRange(bs::network::otc::RangeType range)
{
   switch(range) {
      case RangeType::Range1_5:
         return Range{1, 5};
      case RangeType::Range5_10:
         return Range{5, 10};
      case RangeType::Range10_50:
         return Range{10,50};
      case RangeType::Range50_100:
         return Range{50, 100};
      case RangeType::Range100_250:
         return Range{100, 250};
      case RangeType::Range250plus:
         return Range{250, 1000000};
      case RangeType::Count:
         break;
   }

   assert(false);
   return Range{0, 0};
}

bool bs::network::otc::isValidSide(bs::network::otc::Side side)
{
   switch (side) {
      case Side::Buy:
      case Side::Sell:
         return true;
      default:
         return false;
   }
}

bs::network::otc::Side bs::network::otc::switchSide(bs::network::otc::Side side)
{
   switch (side) {
      case Side::Sell:   return Side::Buy;
      case Side::Buy:    return Side::Sell;
      default:           return Side::Unknown;
   }
}

std::string bs::network::otc::toString(bs::network::otc::State state)
{
   switch (state) {
      case State::OfferSent:        return "OfferSent";
      case State::OfferRecv:        return "OfferRecv";
      case State::WaitAcceptAck:    return "WaitAcceptAck";
      case State::Idle:             return "Idle";
      case State::Blacklisted:      return "Blacklisted";
   }

   assert(false);
   return "";
}

bool bs::network::otc::Offer::operator==(const Offer &other) const
{
   return ourSide == other.ourSide
      && amount == other.amount
      && price == other.price;
}

bool bs::network::otc::Offer::operator!=(const Offer &other) const
{
   return !(*this == other);
}
