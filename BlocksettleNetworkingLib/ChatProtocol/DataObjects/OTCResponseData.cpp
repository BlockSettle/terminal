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
                      , const QString& serverRequestId
                      , const QString& requestorId
                      , const QString& initialTargetId
                      , const QString& responderId
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange)
  : DataObject(DataObject::Type::OTCResponseData)
  , clientResponseId_{clientResponseId}
  , serverResponseId_{}
  , negotiationChannelId_{}
  , serverRequestId_{serverRequestId}
  , requestorId_{requestorId}
  , initialTargetId_{initialTargetId}
  , responderId_{responderId}
  , responseTimestamp_{static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
  , priceRange_{priceRange}
  , quantityRange_{quantityRange}
{}

OTCResponseData::OTCResponseData(const QString& clientResponseId
                      , const QString& serverResponseId
                      , const QString& negotiationChannelId
                      , const QString& serverRequestId
                      , const QString& requestorId
                      , const QString& initialTargetId
                      , const QString& responderId
                      , const uint64_t responseTimestamp
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange)
  : DataObject(DataObject::Type::OTCResponseData)
  , clientResponseId_{clientResponseId}
  , serverResponseId_{serverResponseId}
  , negotiationChannelId_{negotiationChannelId}
  , serverRequestId_{serverRequestId}
  , requestorId_{requestorId}
  , initialTargetId_{initialTargetId}
  , responderId_{responderId}
  , responseTimestamp_{responseTimestamp}
  , priceRange_{priceRange}
  , quantityRange_{quantityRange}
{}

QString OTCResponseData::clientResponseId() const
{
  return clientResponseId_;
}

QString OTCResponseData::serverResponseId() const
{
  return serverResponseId_;
}

QString OTCResponseData::negotiationChannelId() const
{
  return negotiationChannelId_;
}

QString OTCResponseData::serverRequestId() const
{
  return serverRequestId_;
}

QString OTCResponseData::requestorId() const
{
  return requestorId_;
}

QString OTCResponseData::initialTargetId() const
{
  return initialTargetId_;
}

QString OTCResponseData::responderId() const
{
  return responderId_;
}

uint64_t OTCResponseData::responseTimestamp() const
{
  return responseTimestamp_;
}

bs::network::OTCPriceRange    OTCResponseData::priceRange() const
{
  return priceRange_;
}

bs::network::OTCQuantityRange OTCResponseData::quantityRange() const
{
  return quantityRange_;
}


//namespace Chat end
}
