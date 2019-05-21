#ifndef __OTC_UPDATE_DATA_H__
#define __OTC_UPDATE_DATA_H__

#include "DataObject.h"
#include "ChatCommonTypes.h"

namespace Chat {

   class OTCUpdateData : public DataObject
   {
   // DataObject interface
   public:
      QJsonObject toJson() const override;
      static std::shared_ptr<OTCUpdateData> fromJSON(const std::string& jsonData);

   public:
      OTCUpdateData() = delete;

      OTCUpdateData(const QString& serverRequestId
                    , const QString& clientUpdateId
                    , const double amount
                    , const double price);

      OTCUpdateData(const QString& serverRequestId
                    , const QString& clientUpdateId
                    , const QString& serverUpdateId
                    , const uint64_t updateTimestamp
                    , const double amount
                    , const double price);

      ~OTCUpdateData() override = default;

      QString serverRequestId() const;

      QString clientUpdateId() const;
      QString serverUpdateId() const;

      uint64_t updateTimestamp() const;

      double   amount() const;
      double   price() const;
   private:
      const QString  serverRequestId_;
      const QString  clientUpdateId_;
      const QString  serverUpdateId_;
      const uint64_t updateTimestamp_;
      const double   amount_;
      const double   price_;
   };
}

#endif // __OTC_UPDATE_DATA_H__
