#include "OtcUtils.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "otc.pb.h"

using namespace Blocksettle::Communication;
using namespace bs::network::otc;

namespace {

   const std::string kSerializePrefix = "OTC:";

   QString formatResponse(const Otc::ContactMessage::QuoteResponse &response)
   {
      return QStringLiteral("%1-%2 XBT %3-%4 EUR")
         .arg(response.amount().lower())
         .arg(response.amount().upper())
         .arg(UiUtils::displayPriceXBT(fromCents(response.price().lower())))
         .arg(UiUtils::displayPriceXBT(fromCents(response.price().upper())));
   }

   QString formatOffer(const Otc::ContactMessage::Offer &offer)
   {
      return QStringLiteral("%1 XBT - %2 EUR")
         .arg(UiUtils::displayAmount(offer.amount()))
         .arg(UiUtils::displayPriceXBT(fromCents(offer.price())));
   }

} // namespace

std::string OtcUtils::serializeMessage(const BinaryData &data)
{
   return kSerializePrefix + data.toHexStr();
}

BinaryData OtcUtils::deserializeMessage(const std::string &data)
{
   size_t pos = data.find(kSerializePrefix);
   if (pos != 0) {
      return {};
   }
   try {
      return BinaryData::CreateFromHex(data.substr(kSerializePrefix.size()));
   } catch(...) {
      return {};
   }
}

std::string OtcUtils::serializePublicMessage(const BinaryData &data)
{
   return data.toHexStr();
}

BinaryData OtcUtils::deserializePublicMessage(const std::string &data)
{
   try {
      return BinaryData::CreateFromHex(data);
   } catch(...) {
      return {};
   }
}

QString OtcUtils::toReadableString(const QString &text)
{
   auto msgData = OtcUtils::deserializeMessage(text.toStdString());
   if (msgData.isNull()) {
      return {};
   }

   Otc::ContactMessage msg;
   bool result = msg.ParseFromArray(msgData.getPtr(), int(msgData.getSize()));
   if (!result) {
      return {};
   }

   switch (msg.data_case()) {
      case Otc::ContactMessage::kQuoteResponse:
         return QObject::tr("OTC RESPONSE - XBT/EUR - %1").arg(formatResponse(msg.quote_response()));
      case Otc::ContactMessage::kBuyerOffers:
         return QObject::tr("OTC REQUEST - XBT/EUR - BUY - %1").arg(formatOffer(msg.buyer_offers().offer()));
      case Otc::ContactMessage::kSellerOffers:
         return QObject::tr("OTC REQUEST - XBT/EUR - SELL - %1").arg(formatOffer(msg.seller_offers().offer()));
      case Otc::ContactMessage::kBuyerAccepts:
         return QObject::tr("OTC ACCEPT - XBT/EUR - BUY - %1").arg(formatOffer(msg.buyer_accepts().offer()));
      case Otc::ContactMessage::kSellerAccepts:
         return QObject::tr("OTC ACCEPT - XBT/EUR - SELL - %1").arg(formatOffer(msg.seller_accepts().offer()));
      case Otc::ContactMessage::kBuyerAcks:
         return QObject::tr("OTC ACKS");
      case Otc::ContactMessage::kClose:
         return QObject::tr("OTC CANCEL");
      case Otc::ContactMessage::DATA_NOT_SET:
         return QObject::tr("OTC INVALID MESSAGE");
   }

   assert(false);
   return {};
}
