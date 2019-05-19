#include "OTCResponseData.h"

#include <QDateTime>

namespace Chat {

QJsonObject OTCResponseData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   return data;
}

std::shared_ptr<OTCResponseData> OTCResponseData::fromJSON(const std::string& jsonData)
{
   return nullptr;
}

OTCResponseData::OTCResponseData(const QString& clientResponseId
                      , const QString& requestId
                      , const QString& requestorId
                      , const QString& initialTargetId
                      , const QString& responderId
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange)
  : DataObject(DataObject::Type::OTCResponseData)
  , clientResponseId_{clientResponseId}
  , serverResponseId_{}
  , requestId_{requestId}
  , requestorId_{requestorId}
  , initialTargetId_{initialTargetId}
  , responderId_{responderId}
  , responseTimestamp_{static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
  , priceRange_{priceRange}
  , quantityRange_{quantityRange}
{}

OTCResponseData::OTCResponseData(const QString& clientResponseId
                      , const QString& serverResponseId
                      , const QString& requestId
                      , const QString& requestorId
                      , const QString& initialTargetId
                      , const QString& responderId
                      , const uint64_t responseTimestamp
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange)
  : DataObject(DataObject::Type::OTCResponseData)
  , clientResponseId_{clientResponseId}
  , serverResponseId_{serverResponseId}
  , requestId_{requestId}
  , requestorId_{requestorId}
  , initialTargetId_{initialTargetId}
  , responderId_{responderId}
  , responseTimestamp_{responseTimestamp}
  , priceRange_{priceRange}
  , quantityRange_{quantityRange}
{}

//namespace Chat end
}
