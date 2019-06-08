#include "OTCUpdateData.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

namespace Chat {

QJsonObject OTCUpdateData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   data[OTCResponseIdServerKey] = QString::fromStdString(serverResponseId_);
   data[OTCUpdateIdClientKey] = QString::fromStdString(clientUpdateId_);
   data[OTCUpdateSenderIdKey] = QString::fromStdString(updateSenderId_);
   data[OTCUpdateReceiverIdKey] = QString::fromStdString(updateReceiverId_);
   data[OTCUpdateTimestampKey] = QString::number(updateTimestamp_);
   data[OTCUpdateAmountKey] = amount_;
   data[OTCUpdatePriceKey] = price_;

   return data;
}

std::shared_ptr<OTCUpdateData> OTCUpdateData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   const std::string serverResponseId = data[OTCResponseIdServerKey].toString().toStdString();
   const std::string clientUpdateId = data[OTCUpdateIdClientKey].toString().toStdString();
   const std::string updateSenderId = data[OTCUpdateSenderIdKey].toString().toStdString();
   const std::string updateReceiverId = data[OTCUpdateReceiverIdKey].toString().toStdString();
   uint64_t updateTimestamp = data[OTCUpdateTimestampKey].toString().toULongLong();
   double amount = data[OTCUpdateAmountKey].toDouble();
   double price = data[OTCUpdatePriceKey].toDouble();

   return std::make_shared<OTCUpdateData>(serverResponseId,
                                          clientUpdateId,
                                          updateSenderId,
                                          updateReceiverId,
                                          updateTimestamp,
                                          amount,
                                          price);
}

OTCUpdateData::OTCUpdateData(  const std::string& serverResponseId
                             , const std::string& clientUpdateId
                             , const std::string& updateSenderId
                             , const double amount
                             , const double price)
 : OTCUpdateData(serverResponseId, clientUpdateId, updateSenderId, ""
                 , static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())
                 , amount, price)
{}

OTCUpdateData::OTCUpdateData(  const std::string& serverResponseId
                             , const std::string& clientUpdateId
                             , const std::string& updateSenderId
                             , const std::string& updateReceiverId
                             , const uint64_t updateTimestamp
                             , const double amount
                             , const double price)
  : DataObject(DataObject::Type::OTCUpdateData)
  , serverResponseId_{serverResponseId}
  , clientUpdateId_{clientUpdateId}
  , updateSenderId_{updateSenderId}
  , updateReceiverId_{updateReceiverId}
  , updateTimestamp_{updateTimestamp}
  , amount_{amount}
  , price_{price}
{}

std::string OTCUpdateData::serverResponseId() const
{
   return serverResponseId_;
}

std::string OTCUpdateData::clientUpdateId() const
{
   return clientUpdateId_;
}

std::string OTCUpdateData::updateSenderId() const
{
   return updateSenderId_;
}

std::string OTCUpdateData::updateReceiverId() const
{
   return updateReceiverId_;
}

void OTCUpdateData::setUpdateReceiverId(const std::string& receiverId)
{
   updateReceiverId_ = receiverId;
}

uint64_t OTCUpdateData::updateTimestamp() const
{
   return updateTimestamp_;
}

void OTCUpdateData::setCurrentUpdateTimestamp()
{
   updateTimestamp_ = static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
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
