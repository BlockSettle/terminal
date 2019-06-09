#include "OTCRequestData.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Chat
{
   OTCRequestData::OTCRequestData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const bs::network::OTCRequest& otcRequest,
         int state)
   : MessageData(sender, receiver, id, dateTime, serializeRequestData(otcRequest), MessageData::RawMessageDataType::OTCReqeust, state)
   , requestValid_{true}
   , otcRequest_{otcRequest}
   {
      updateDisplayString();
   }

   OTCRequestData::OTCRequestData(const MessageData& source, const QJsonObject& jsonData)
      : MessageData(source, RawMessageDataType::OTCReqeust)
   {
      otcRequest_.side = static_cast<bs::network::ChatOTCSide::Type>(jsonData[QLatin1String("side")].toInt());
      otcRequest_.amountRange = static_cast<bs::network::OTCRangeID::Type>(jsonData[QLatin1String("amount")].toInt());
      requestValid_ = true;

      updateDisplayString();
   }

   void OTCRequestData::messageDirectionUpdate()
   {
      updateDisplayString();
   }

   QString OTCRequestData::displayText() const
   {
      return displayText_;
   }

   bool OTCRequestData::otcReqeustValid()
   {
      return requestValid_;
   }

   bs::network::OTCRequest OTCRequestData::otcRequest() const
   {
      return otcRequest_;
   }

   QString OTCRequestData::serializeRequestData(const bs::network::OTCRequest otcRequest)
   {
      QJsonObject data;

      data[QLatin1String("raw_message_type")] = static_cast<int>(RawMessageDataType::OTCReqeust);
      data[QLatin1String("side")] = static_cast<int>(otcRequest.side);
      data[QLatin1String("amount")] = static_cast<int>(otcRequest.amountRange);

      QJsonDocument doc(data);
      return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
   }

   void OTCRequestData::updateDisplayString()
   {
      if (requestValid_) {
         QString format = QLatin1String("%1 OTC request: %2 %3 EUR/XBT");

         displayText_ = format
            .arg(directionToText(messageDirectoin()))
            .arg(QString::fromStdString(bs::network::ChatOTCSide::toString(otcRequest_.side)))
            .arg(QString::fromStdString(bs::network::OTCRangeID::toString(otcRequest_.amountRange)));
      } else {
         displayText_ = QLatin1String("Invalid");
      }
   }
//namespace Chat end
}
