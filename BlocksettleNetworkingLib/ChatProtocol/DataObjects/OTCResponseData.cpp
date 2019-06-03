#include "OTCResponseData.h"

#include "../ProtocolDefinitions.h"

#include <QDateTime>

namespace Chat {

QJsonObject OTCResponseData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   data[OTCResponseIdClientKey] = QString::fromStdString(clientResponseId_);
   data[OTCResponseIdServerKey] = QString::fromStdString(serverResponseId_);
   data[OTCRequestIdServerKey] = QString::fromStdString(serverRequestId_);
   data[OTCRequestorIdKey] = QString::fromStdString(requestorId_);
   data[OTCTargetIdKey] = QString::fromStdString(initialTargetId_);
   data[OTCResponderIdKey] = QString::fromStdString(responderId_);
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

   const auto clientResponseId = data[OTCResponseIdClientKey].toString().toStdString();
   const auto serverResponseId = data[OTCResponseIdServerKey].toString().toStdString();
   const auto serverRequestId = data[OTCRequestIdServerKey].toString().toStdString();
   const auto requestorId = data[OTCRequestorIdKey].toString().toStdString();
   const auto initialTargetId = data[OTCTargetIdKey].toString().toStdString();
   const auto responderId = data[OTCResponderIdKey].toString().toStdString();
   uint64_t responseTimestamp = data[OTCResponseTimestampKey].toString().toULongLong();

   QJsonObject pro = data[OTCPriceRangeObjectKey].toObject();
   bs::network::OTCPriceRange priceRange = {pro[OTCLowerKey].toString().toULongLong(),
                                            pro[OTCUpperKey].toString().toULongLong()};

   QJsonObject qro = data[OTCQuantityRangeObjectKey].toObject();
   bs::network::OTCQuantityRange quantityRange =
   {qro[OTCLowerKey].toString().toULongLong(), qro[OTCUpperKey].toString().toULongLong()};

   return std::make_shared<OTCResponseData>(clientResponseId,
                                           serverResponseId,
                                           serverRequestId,
                                           requestorId,
                                           initialTargetId,
                                           responderId,
                                           responseTimestamp,
                                           priceRange,
                                           quantityRange);
}

OTCResponseData::OTCResponseData(const std::string& clientResponseId
                      , const std::string& serverRequestId
                      , const std::string& requestorId
                      , const std::string& initialTargetId
                      , const std::string& responderId
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange)
  : DataObject(DataObject::Type::OTCResponseData)
  , clientResponseId_{clientResponseId}
  , serverResponseId_{}
  , serverRequestId_{serverRequestId}
  , requestorId_{requestorId}
  , initialTargetId_{initialTargetId}
  , responderId_{responderId}
  , responseTimestamp_{static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
  , priceRange_{priceRange}
  , quantityRange_{quantityRange}
{}

OTCResponseData::OTCResponseData(const std::string& clientResponseId
                      , const std::string& serverResponseId
                      , const std::string& serverRequestId
                      , const std::string& requestorId
                      , const std::string& initialTargetId
                      , const std::string& responderId
                      , const uint64_t responseTimestamp
                      , const bs::network::OTCPriceRange& priceRange
                      , const bs::network::OTCQuantityRange& quantityRange)
  : DataObject(DataObject::Type::OTCResponseData)
  , clientResponseId_{clientResponseId}
  , serverResponseId_{serverResponseId}
  , serverRequestId_{serverRequestId}
  , requestorId_{requestorId}
  , initialTargetId_{initialTargetId}
  , responderId_{responderId}
  , responseTimestamp_{responseTimestamp}
  , priceRange_{priceRange}
  , quantityRange_{quantityRange}
{}

std::string OTCResponseData::clientResponseId() const
{
  return clientResponseId_;
}

std::string OTCResponseData::serverResponseId() const
{
  return serverResponseId_;
}

std::string OTCResponseData::serverRequestId() const
{
  return serverRequestId_;
}

std::string OTCResponseData::requestorId() const
{
  return requestorId_;
}

std::string OTCResponseData::initialTargetId() const
{
  return initialTargetId_;
}

std::string OTCResponseData::responderId() const
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

void OTCResponseData::setServerResponseId(const std::string &serverResponseId)
{
    serverResponseId_ = serverResponseId;
}


//namespace Chat end
    }
