#include "CommonTypes.h"
#include "SettlementMonitor.h"

Q_DECLARE_METATYPE(bs::PayoutSignatureType)

using namespace bs::network;

class CommonTypesMetaRegistration
{
public:
   CommonTypesMetaRegistration()
   {
      qRegisterMetaType<bs::network::Asset::Type>("AssetType");
      qRegisterMetaType<bs::network::Quote>("Quote");
      qRegisterMetaType<bs::network::Order>("Order");
      qRegisterMetaType<bs::network::SecurityDef>("SecurityDef");
      qRegisterMetaType<bs::network::QuoteReqNotification>("QuoteReqNotification");
      qRegisterMetaType<bs::network::QuoteNotification>("QuoteNotification");
      qRegisterMetaType<bs::network::MDField>("MDField");
      qRegisterMetaType<bs::network::MDFields>("MDFields");
      qRegisterMetaType<bs::network::CCSecurityDef>("CCSecurityDef");
      qRegisterMetaType<bs::network::NewTrade>("NewTrade");
      qRegisterMetaType<bs::network::NewPMTrade>("NewPMTrade");
      qRegisterMetaType<bs::PayoutSignatureType>();
   }

   ~CommonTypesMetaRegistration() noexcept = default;

   CommonTypesMetaRegistration(const CommonTypesMetaRegistration&) = delete;
   CommonTypesMetaRegistration& operator = (const CommonTypesMetaRegistration&) = delete;

   CommonTypesMetaRegistration(CommonTypesMetaRegistration&&) = delete;
   CommonTypesMetaRegistration& operator = (CommonTypesMetaRegistration&&) = delete;
};

static CommonTypesMetaRegistration commonTypesRegistrator{};

bool RFQ::isXbtBuy() const
{
   if (assetType != Asset::SpotXBT) {
      return false;
   }
   return (product != XbtCurrency) ? (side == Side::Sell) : (side == Side::Buy);
}


QuoteNotification::QuoteNotification(const QuoteReqNotification &qrn, const std::string &_authKey, double price
   , const std::string &txData)
   : authKey(_authKey), reqAuthKey(qrn.requestorAuthPublicKey), settlementId(qrn.settlementId), sessionToken(qrn.sessionToken)
   , quoteRequestId(qrn.quoteRequestId), security(qrn.security), product(qrn.product), account(qrn.account), transactionData(txData)
   , assetType(qrn.assetType), validityInS(120), bidFwdPts(0), bidContraQty(0), offerFwdPts(0), offerContraQty(0)
{
   const auto &baseProduct = security.substr(0, security.find('/'));
   side = bs::network::Side::invert(qrn.side);

   if ((side == bs::network::Side::Buy) || ((side == bs::network::Side::Sell) && (product != baseProduct))) {
      bidPx = offerPx = price;
      bidSz = offerSz = qrn.quantity;
   }
   else {
      offerPx = bidPx = price;
      offerSz = bidSz = qrn.quantity;
   }
}


MDField::Type MDField::fromCeler(com::celertech::marketdata::api::enums::marketdataentrytype::MarketDataEntryType mdType) {
   switch (mdType)
   {
   case com::celertech::marketdata::api::enums::marketdataentrytype::BID:       return PriceBid;
   case com::celertech::marketdata::api::enums::marketdataentrytype::OFFER:     return PriceOffer;
   case com::celertech::marketdata::api::enums::marketdataentrytype::MID_PRICE: return PriceMid;
   case com::celertech::marketdata::api::enums::marketdataentrytype::TRADE:     return PriceLast;
   case com::celertech::marketdata::api::enums::marketdataentrytype::OPEN:      return PriceOpen;
   case com::celertech::marketdata::api::enums::marketdataentrytype::CLOSE:     return PriceClose;
   case com::celertech::marketdata::api::enums::marketdataentrytype::HIGH:      return PriceHigh;
   case com::celertech::marketdata::api::enums::marketdataentrytype::LOW:       return PriceLow;
   case com::celertech::marketdata::api::enums::marketdataentrytype::TOTALTRADEDVOL:  return TurnOverQty;
   case com::celertech::marketdata::api::enums::marketdataentrytype::SETTLEMENT:return PriceSettlement;
   case com::celertech::marketdata::api::enums::marketdataentrytype::VWAP:      return VWAP;
   default:       return Unknown;
   }
}

MDField MDField::get(const MDFields &fields, MDField::Type type) {
   for (const auto &field : fields) {
      if (field.type == type) {
         return field;
      }
   }
   return { MDField::Unknown, 0, QString() };
}
