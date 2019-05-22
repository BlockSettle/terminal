#include "OTCRequestData.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

namespace Chat {

QJsonObject OTCRequestData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   data[OTCRequestIdClientKey] = clientRequestId_;
   data[OTCRequestIdServerKey] = serverRequestId_;
   data[OTCRequestorIdKey] = requestorId_;
   data[OTCTargetIdKey] = targetId_;
   data[OTCSubmitTimestampKey] = QString::number(submitTimestamp_);
   data[OTCExpiredTimestampKey] = QString::number(expireTimestamp_);
   data[OTCRqSideKey] = static_cast<int>(otcRequest_.side);
   data[OTCRqRangeIdKey] = static_cast<int>(otcRequest_.amountRange);

   return data;
}

std::shared_ptr<OTCRequestData> OTCRequestData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString clientRequestId = data[OTCRequestIdClientKey].toString();
   QString serverRequestId = data[OTCRequestIdServerKey].toString();
   QString requestorId = data[OTCRequestorIdKey].toString();
   QString targetId = data[OTCTargetIdKey].toString();
   uint64_t submitTimestamp = data[OTCSubmitTimestampKey].toString().toULongLong();
   uint64_t expireTimestamp = data[OTCExpiredTimestampKey].toString().toULongLong();
   bs::network::ChatOTCSide::Type side =
         static_cast<bs::network::ChatOTCSide::Type>(data[OTCRqSideKey].toInt());
   bs::network::OTCRangeID::Type range =
         static_cast<bs::network::OTCRangeID::Type>(data[OTCRqRangeIdKey].toInt());

   bs::network::OTCRequest otcRq{side, range, false, false};

   return std::make_shared<OTCRequestData>(clientRequestId,
                                           serverRequestId,
                                           requestorId,
                                           targetId,
                                           submitTimestamp,
                                           expireTimestamp,
                                           otcRq);
}

OTCRequestData::OTCRequestData(const QString& clientRequestId
                               , const QString& requestorId
                               , const QString& targetId
                               , const bs::network::OTCRequest& otcRequest)
  : DataObject(DataObject::Type::OTCRequestData)
  , clientRequestId_{clientRequestId}
  , serverRequestId_{} // empty string
  , requestorId_{requestorId}
  , targetId_{targetId}
  , submitTimestamp_{static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
  , expireTimestamp_{0}
  , otcRequest_{otcRequest}
{}

OTCRequestData::OTCRequestData(const QString& clientRequestId, const QString& serverRequestId
                               , const QString& requestorId, const QString& targetId
                               , uint64_t submitTimestamp, uint64_t expireTimestamp
                               , const bs::network::OTCRequest& otcRequest)
  : DataObject(DataObject::Type::OTCRequestData)
  , clientRequestId_{clientRequestId}
  , serverRequestId_{serverRequestId}
  , requestorId_{requestorId}
  , targetId_{targetId}
  , submitTimestamp_{submitTimestamp}
  , expireTimestamp_{expireTimestamp}
  , otcRequest_{otcRequest}
{}

QString OTCRequestData::clientRequestId() const
{
   return clientRequestId_;
}

QString OTCRequestData::serverRequestId() const
{
   return serverRequestId_;
}

QString OTCRequestData::requestorId() const
{
   return requestorId_;
}

QString OTCRequestData::targetId() const
{
   return targetId_;
}

uint64_t OTCRequestData::submitTimestamp() const
{
   return submitTimestamp_;
}

uint64_t OTCRequestData::expireTimestamp() const
{
   return expireTimestamp_;
}

const bs::network::OTCRequest& OTCRequestData::otcRequest() const
{
   return otcRequest_;
}

//namespace Chat end
}
