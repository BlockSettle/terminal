#include "CommonTypes.h"

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
