#include "OTCUpdateData.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

namespace Chat {

QJsonObject OTCUpdateData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   data[OTCRequestIdServerKey] = serverRequestId_;
   data[OTCUpdateIdClientKey] = clientUpdateId_;
   data[OTCUpdateIdServerKey] = serverUpdateId_;
   data[OTCUpdateTimestampKey] = QString::number(updateTimestamp_);
   data[OTCUpdateAmountKey] = amount_;
   data[OTCUpdatePriceKey] = price_;
}

std::shared_ptr<OTCUpdateData> OTCUpdateData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString serverRequestId = data[OTCRequestIdServerKey].toString();
   QString clientUpdateId = data[OTCUpdateIdClientKey].toString();
   QString serverUpdateId = data[OTCUpdateIdServerKey].toString();
   uint64_t updateTimestamp = data[OTCUpdateTimestampKey].toString().toULongLong();
   double amount = data[OTCUpdateAmountKey].toDouble();
   double price = data[OTCUpdatePriceKey].toDouble();

   return std::make_shared<OTCUpdateData>(serverRequestId,
                                          clientUpdateId,
                                          serverUpdateId,
                                          updateTimestamp,
                                          amount,
                                          price);
}

OTCUpdateData::OTCUpdateData(  const QString& serverRequestId
                             , const QString& clientUpdateId
                             , const double amount
                             , const double price)
  : DataObject(DataObject::Type::OTCUpdateData)
  , serverRequestId_{serverRequestId}
  , clientUpdateId_{clientUpdateId}
  , serverUpdateId_{}
  , updateTimestamp_{static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
  , amount_{amount}
  , price_{price}
{}

OTCUpdateData::OTCUpdateData(  const QString& serverRequestId
                             , const QString& clientUpdateId
                             , const QString& serverUpdateId
                             , const uint64_t updateTimestamp
                             , const double amount
                             , const double price)
  : DataObject(DataObject::Type::OTCUpdateData)
  , serverRequestId_{serverRequestId}
  , clientUpdateId_{clientUpdateId}
  , serverUpdateId_{serverUpdateId}
  , updateTimestamp_{updateTimestamp}
  , amount_{amount}
  , price_{price}
{}

QString OTCUpdateData::serverRequestId() const
{
   return serverRequestId_;
}

QString OTCUpdateData::clientUpdateId() const
{
   return clientUpdateId_;
}

QString OTCUpdateData::serverUpdateId() const
{
   return serverUpdateId_;
}

uint64_t OTCUpdateData::updateTimestamp() const
{
   return updateTimestamp_;
}

double OTCUpdateData::amount() const
{
   return amount_;
}

double OTCUpdateData::price() const
{
   return price_;
}

//namespace Chat end
}
