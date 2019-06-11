#include "OTCCloseTradingData.h"

#include "../ProtocolDefinitions.h"
#include <QDateTime>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Chat
{
   OTCCloseTradingData::OTCCloseTradingData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         int state)
   : MessageData(sender, receiver, id, dateTime, serializeCloseRequest(), MessageData::RawMessageDataType::OTCCloseTrading, state)
   {
      updateDisplayString();
   }

   OTCCloseTradingData::OTCCloseTradingData(const MessageData& source, const QJsonObject& jsonData)
      : MessageData(source, RawMessageDataType::OTCCloseTrading)
   {
      updateDisplayString();
   }

   void OTCCloseTradingData::messageDirectionUpdate()
   {
      updateDisplayString();
   }

   QString OTCCloseTradingData::displayText() const
   {
      return displayText_;
   }

   QString OTCCloseTradingData::serializeCloseRequest()
   {
      QJsonObject data;

      data[QLatin1String("raw_message_type")] = static_cast<int>(RawMessageDataType::OTCCloseTrading);

      QJsonDocument doc(data);
      return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
   }

   void OTCCloseTradingData::updateDisplayString()
   {
      QString format = QLatin1String("%1 CLOSE");

      displayText_ = format.arg(directionToText(messageDirectoin()));
   }
//namespace Chat end
}
