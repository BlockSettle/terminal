#include "OTCResponseData.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

//const bs::network::OTCRequest& otcRequest
// data[OTCRqSideKey] = static_cast<int>(otcRequest_.side);
// data[OTCRqRangeIdKey] = static_cast<int>(otcRequest_.amountRange);

namespace Chat
{
   OTCResponseData::OTCResponseData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const bs::network::OTCResponse& otcResponse,
         int state)
   : MessageData(sender, receiver, id, dateTime, serializeResponseData(otcResponse), MessageData::RawMessageDataType::OTCResponse, state)
   , resopnseValid_{true}
   , otcResponse_{otcResponse}
   {
      updateDisplayString();
   }

   OTCResponseData::OTCResponseData(const MessageData& source, const QJsonObject& jsonData)
      : MessageData(source, RawMessageDataType::OTCResponse)
   {
      otcResponse_.priceRange.lower = jsonData[QLatin1String("price_low")].toInt();
      otcResponse_.priceRange.upper = jsonData[QLatin1String("price_high")].toInt();
      otcResponse_.quantityRange.lower = jsonData[QLatin1String("amount_low")].toInt();
      otcResponse_.quantityRange.upper = jsonData[QLatin1String("amount_high")].toInt();

      resopnseValid_ = true;

      updateDisplayString();
   }

   void OTCResponseData::messageDirectionUpdate()
   {
      updateDisplayString();
   }

   QString OTCResponseData::displayText() const
   {
      return displayText_;
   }

   bool OTCResponseData::otcResponseValid()
   {
      return resopnseValid_;
   }

   bs::network::OTCResponse OTCResponseData::otcResponse() const
   {
      return otcResponse_;
   }

   QString OTCResponseData::serializeResponseData(const bs::network::OTCResponse otcResponse)
   {
      QJsonObject data;

      data[QLatin1String("raw_message_type")] = static_cast<int>(RawMessageDataType::OTCReqeust);

      data[QLatin1String("price_low")] = static_cast<int>(otcResponse.priceRange.lower);
      data[QLatin1String("price_high")] = static_cast<int>(otcResponse.priceRange.upper);
      data[QLatin1String("amount_low")] = static_cast<int>(otcResponse.quantityRange.lower);
      data[QLatin1String("amount_high")] = static_cast<int>(otcResponse.quantityRange.upper);

      QJsonDocument doc(data);
      return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
   }

   void OTCResponseData::updateDisplayString()
   {
      if (resopnseValid_) {
         QString format = QLatin1String("%1 BID : %2-%3 EUR @ %4-%5 XBT");

         displayText_ = format
            .arg(directionToText(messageDirectoin()))
            .arg(QString::number(otcResponse_.priceRange.lower))
            .arg(QString::number(otcResponse_.priceRange.upper))
            .arg(QString::number(otcResponse_.quantityRange.lower))
            .arg(QString::number(otcResponse_.quantityRange.upper));
      } else {
         displayText_ = QLatin1String("Invalid");
      }
   }
//namespace Chat end
}
