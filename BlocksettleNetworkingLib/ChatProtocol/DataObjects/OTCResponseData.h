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

      OTCResponseData(const std::string& clientResponseId
                      , const std::string& serverRequestId
                      , const std::string& requestorId
                      , const std::string& initialTargetId
                      , const std::string& responderId
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange);

      OTCResponseData(const std::string& clientResponseId
                      , const std::string& serverResponseId
                      , const std::string& serverRequestId
                      , const std::string& requestorId
                      , const std::string& initialTargetId
                      , const std::string& responderId
                      , const uint64_t responseTimestamp
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange);

      ~OTCResponseData() override = default;

      std::string clientResponseId() const;
      std::string serverResponseId() const;

      std::string serverRequestId() const;

      // requestorId - who sent request
      std::string requestorId() const;
      // initialTargetId - where it was sent
      std::string initialTargetId() const;

      //responderId - current user id
      std::string responderId() const;

      uint64_t responseTimestamp() const;

      bs::network::OTCPriceRange    priceRange() const;
      bs::network::OTCQuantityRange quantityRange() const;

      void setServerResponseId(const std::string &serverResponseId);

   private:
      const std::string clientResponseId_;
      std::string serverResponseId_;
      const std::string serverRequestId_;
      const std::string requestorId_;
      const std::string initialTargetId_;
      const std::string responderId_;
      const uint64_t responseTimestamp_;
      const bs::network::OTCPriceRange    priceRange_;
      const bs::network::OTCQuantityRange quantityRange_;
   };

}

#endif // __OTC_RESPONSE_DATA_H__
