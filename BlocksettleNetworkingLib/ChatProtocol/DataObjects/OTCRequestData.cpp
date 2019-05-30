#include "OTCRequestData.h"

#include <QDateTime>

namespace Chat {

QJsonObject OTCRequestData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   return data;
}

std::shared_ptr<OTCRequestData> OTCRequestData::fromJSON(const std::string& jsonData)
{
   return nullptr;
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
