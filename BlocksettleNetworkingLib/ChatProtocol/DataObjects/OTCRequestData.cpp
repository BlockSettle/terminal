#include "OTCRequestData.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

namespace Chat {

QJsonObject OTCRequestData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   data[OTCRequestIdClientKey] = QString::fromStdString(clientRequestId_);
   data[OTCRequestIdServerKey] = QString::fromStdString(serverRequestId_);
   data[OTCRequestorIdKey] = QString::fromStdString(requestorId_);
   data[OTCTargetIdKey] = QString::fromStdString(targetId_);
   data[OTCSubmitTimestampKey] = QString::number(submitTimestamp_);
   data[OTCExpiredTimestampKey] = QString::number(expireTimestamp_);
   data[OTCRqSideKey] = static_cast<int>(otcRequest_.side);
   data[OTCRqRangeIdKey] = static_cast<int>(otcRequest_.amountRange);

   return data;
}

std::shared_ptr<OTCRequestData> OTCRequestData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   const auto clientRequestId = data[OTCRequestIdClientKey].toString().toStdString();
   const auto serverRequestId = data[OTCRequestIdServerKey].toString().toStdString();
   const auto requestorId = data[OTCRequestorIdKey].toString().toStdString();
   const auto targetId = data[OTCTargetIdKey].toString().toStdString();
   uint64_t submitTimestamp = data[OTCSubmitTimestampKey].toString().toULongLong();
   uint64_t expireTimestamp = data[OTCExpiredTimestampKey].toString().toULongLong();
   bs::network::ChatOTCSide::Type side =
         static_cast<bs::network::ChatOTCSide::Type>(data[OTCRqSideKey].toInt());
   bs::network::OTCRangeID::Type range =
         static_cast<bs::network::OTCRangeID::Type>(data[OTCRqRangeIdKey].toInt());

   bs::network::OTCRequest otcRq{side, range};

   return std::make_shared<OTCRequestData>(clientRequestId,
                                           serverRequestId,
                                           requestorId,
                                           targetId,
                                           submitTimestamp,
                                           expireTimestamp,
                                           otcRq);
}

OTCRequestData::OTCRequestData(const std::string& clientRequestId
                               , const std::string& requestorId
                               , const std::string& targetId
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

OTCRequestData::OTCRequestData(const std::string& clientRequestId, const std::string& serverRequestId
                               , const std::string& requestorId, const std::string& targetId
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

std::string OTCRequestData::clientRequestId() const
{
   return clientRequestId_;
}

std::string OTCRequestData::serverRequestId() const
{
   return serverRequestId_;
}

std::string OTCRequestData::requestorId() const
{
   return requestorId_;
}

std::string OTCRequestData::targetId() const
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

void OTCRequestData::setServerRequestId(const std::string &serverRequestId)
{
    serverRequestId_ = serverRequestId;
}

void OTCRequestData::setExpireTimestamp(const uint64_t &expireTimestamp)
{
    expireTimestamp_ = expireTimestamp;
}

//namespace Chat end
    }
