#include "OTCResponseData.h"

#include "../ProtocolDefinitions.h"

#include <QDateTime>

namespace Chat {

QJsonObject OTCResponseData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   data[OTCResponseIdClientKey] = clientResponseId_;
   data[OTCResponseIdServerKey] = serverResponseId_;
   data[OTCNegotiationChannelIdKey] = negotiationChannelId_;
   data[OTCRequestIdServerKey] = serverRequestId_;
   data[OTCRequestorIdKey] = requestorId_;
   data[OTCTargetIdKey] = initialTargetId_;
   data[OTCResponderIdKey] = responderId_;
   data[OTCResponseTimestampKey] = QString::number(responseTimestamp_);

   QJsonObject priceRangeObj =  {{OTCLowerKey, QString::number(priceRange_.lower)},
                                 {OTCUpperKey, QString::number(priceRange_.upper)}};
   data[OTCPriceRangeObjectKey] = priceRangeObj;

   QJsonObject quantityRangeObj = {{OTCLowerKey, QString::number(quantityRange_.lower)},
                                   {OTCUpperKey, QString::number(quantityRange_.upper)}};
   data[OTCQuantityRangeObjectKey] = quantityRangeObj;

   return data;
}

std::shared_ptr<OTCResponseData> OTCResponseData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString clientResponseId = data[OTCResponseIdClientKey].toString();
   QString serverResponseId = data[OTCResponseIdServerKey].toString();
   QString negotiationChannelId = data[OTCNegotiationChannelIdKey].toString();
   QString serverRequestId = data[OTCRequestIdServerKey].toString();
   QString requestorId = data[OTCRequestorIdKey].toString();
   QString initialTargetId = data[OTCTargetIdKey].toString();
   QString responderId = data[OTCResponderIdKey].toString();
   uint64_t responseTimestamp = data[OTCResponseTimestampKey].toString().toULongLong();

   QJsonObject pro = data[OTCPriceRangeObjectKey].toObject();
   bs::network::OTCPriceRange priceRange = {pro[OTCLowerKey].toString().toULongLong(),
                                            pro[OTCUpperKey].toString().toULongLong()};

   QJsonObject qro = data[OTCQuantityRangeObjectKey].toObject();
   bs::network::OTCQuantityRange quantityRange =
   {qro[OTCLowerKey].toString().toULongLong(), qro[OTCUpperKey].toString().toULongLong()};

   return std::make_shared<OTCResponseData>(clientResponseId,
                                           serverResponseId,
                                           negotiationChannelId,
                                           serverRequestId,
                                           requestorId,
                                           initialTargetId,
                                           responderId,
                                           responseTimestamp,
                                           priceRange,
                                           quantityRange);
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
