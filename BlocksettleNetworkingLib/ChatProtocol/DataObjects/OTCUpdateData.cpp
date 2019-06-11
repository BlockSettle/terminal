#include "OTCUpdateData.h"

#include "../ProtocolDefinitions.h"
#include <QDateTime>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Chat
{
   OTCUpdateData::OTCUpdateData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const bs::network::OTCUpdate& otcUpdate,
         int state)
   : MessageData(sender, receiver, id, dateTime, serializeUpdateData(otcUpdate), MessageData::RawMessageDataType::OTCUpdate, state)
   , updateValid_{true}
   , otcUpdate_{otcUpdate}
   {
      updateDisplayString();
   }

   OTCUpdateData::OTCUpdateData(const MessageData& source, const QJsonObject& jsonData)
      : MessageData(source, RawMessageDataType::OTCUpdate)
   {
      otcUpdate_.amount = jsonData[QLatin1String("amount")].toInt();
      otcUpdate_.price = jsonData[QLatin1String("price")].toInt();

      updateValid_ = true;

      updateDisplayString();
   }

   void OTCUpdateData::messageDirectionUpdate()
   {
      updateDisplayString();
   }

   QString OTCUpdateData::displayText() const
   {
      return displayText_;
   }

   bool OTCUpdateData::otcUpdateValid()
   {
      return updateValid_;
   }

   bs::network::OTCUpdate OTCUpdateData::otcUpdate() const
   {
      return otcUpdate_;
   }

   QString OTCUpdateData::serializeUpdateData(const bs::network::OTCUpdate& update)
   {
      QJsonObject data;

      data[QLatin1String("raw_message_type")] = static_cast<int>(RawMessageDataType::OTCUpdate);

      data[QLatin1String("amount")] = static_cast<int>(update.amount);
      data[QLatin1String("price")] = static_cast<int>(update.price);

      QJsonDocument doc(data);
      return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
   }

   void OTCUpdateData::updateDisplayString()
   {
      if (updateValid_) {
         QString format = QLatin1String("%1 UPDATE : %2 EUR @ %3 XBT");

         displayText_ = format
            .arg(directionToText(messageDirectoin()))
            .arg(QString::number(otcUpdate_.price))
            .arg(QString::number(otcUpdate_.amount));
      } else {
         displayText_ = QLatin1String("Invalid");
      }
   }
//namespace Chat end
}
