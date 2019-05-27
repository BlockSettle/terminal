#ifndef __OTC_RESPONSE_DATA_H__
#define __OTC_RESPONSE_DATA_H__

#include "DataObject.h"
#include "ChatCommonTypes.h"

namespace Chat {

   class OTCResponseData : public DataObject
   {
   // DataObject interface
   public:
      QJsonObject toJson() const override;
      static std::shared_ptr<OTCResponseData> fromJSON(const std::string& jsonData);

   public:
      OTCResponseData() = delete;

      OTCResponseData(const QString& clientResponseId
                      , const QString& serverRequestId
                      , const QString& requestorId
                      , const QString& initialTargetId
                      , const QString& responderId
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange);

      OTCResponseData(const QString& clientResponseId
                      , const QString& serverResponseId
                      , const QString& negotiationChannelId
                      , const QString& serverRequestId
                      , const QString& requestorId
                      , const QString& initialTargetId
                      , const QString& responderId
                      , const uint64_t responseTimestamp
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange);

      ~OTCResponseData() override = default;

      QString clientResponseId() const;
      QString serverResponseId() const;

      QString negotiationChannelId() const;

      QString serverRequestId() const;

      // requestorId - who sent request
      QString requestorId() const;
      // initialTargetId - where it was sent
      QString initialTargetId() const;

      //responderId - current user id
      QString responderId() const;

      uint64_t responseTimestamp() const;

      bs::network::OTCPriceRange    priceRange() const;
      bs::network::OTCQuantityRange quantityRange() const;

   private:
      const QString clientResponseId_;
      const QString serverResponseId_;
      const QString negotiationChannelId_;
      const QString serverRequestId_;
      const QString requestorId_;
      const QString initialTargetId_;
      const QString responderId_;
      const uint64_t responseTimestamp_;
      const bs::network::OTCPriceRange    priceRange_;
      const bs::network::OTCQuantityRange quantityRange_;
   };

}

#endif // __OTC_RESPONSE_DATA_H__
