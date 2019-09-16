#include "OtcUtils.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "otc.pb.h"

using namespace Blocksettle::Communication;
using namespace bs::network::otc;

namespace {

   const std::string kSerializePrefix = "OTC:";

   QString formatOffer(const Otc::Message::Offer &offer)
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

QString OtcUtils::toReadableString(const QString &text)
{
   auto msgData = OtcUtils::deserializeMessage(text.toStdString());
   if (msgData.isNull()) {
      return {};
   }

   Otc::Message msg;
   bool result = msg.ParseFromArray(msgData.getPtr(), int(msgData.getSize()));
   if (!result) {
      return {};
   }

   switch (msg.data_case()) {
      case Otc::Message::kBuyerOffers:
         return QObject::tr("OTC REQUEST - XBT/EUR - BUY - %1").arg(formatOffer(msg.buyer_offers().offer()));
      case Otc::Message::kSellerOffers:
         return QObject::tr("OTC REQUEST - XBT/EUR - SELL - %1").arg(formatOffer(msg.seller_offers().offer()));
      case Otc::Message::kBuyerAccepts:
         return QObject::tr("OTC ACCEPT - XBT/EUR - BUY - %1").arg(formatOffer(msg.buyer_accepts().offer()));
      case Otc::Message::kSellerAccepts:
         return QObject::tr("OTC ACCEPT - XBT/EUR - SELL - %1").arg(formatOffer(msg.seller_accepts().offer()));
      case Otc::Message::kBuyerAcks:
         return QObject::tr("OTC ACKS");
      case Otc::Message::kClose:
         return QObject::tr("OTC CANCEL");
      case Otc::Message::DATA_NOT_SET:
         return QObject::tr("OTC INVALID MESSAGE");
   }

   assert(false);
   return {};
}
