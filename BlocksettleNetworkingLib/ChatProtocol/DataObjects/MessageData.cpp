#include "MessageData.h"

#include "../ProtocolDefinitions.h"
#include "OTCRequestData.h"
#include "OTCResponseData.h"
#include "OTCUpdateData.h"
#include "OTCCloseTradingData.h"

#include <QDebug>

namespace Chat {

   QString MessageData::serializePayload()
   {
      QJsonObject data;

      data[QLatin1String("raw_message_type")] = static_cast<int>(rawType_);
      data[QLatin1String("message_text")] = displayText_;

      QJsonDocument doc(data);
      return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
   }

   QString MessageData::directionToText(MessageDirection direction)
   {
      switch(direction) {
      case MessageDirection::Sent:
         return QLatin1String("Sent");
      case MessageDirection::Received:
         return QLatin1String("Received");
      default:
         return QLatin1String("<direction not set>");
      }
   }

   const size_t NONCE_SIZE = 24;

   MessageData::MessageData(const QJsonObject& data)
      : DataObject(DataObject::Type::MessageData)
   {
      id_ = data[MessageIdKey].toString();
      senderId_ = data[SenderIdKey].toString();
      state_ = data[StatusKey].toInt();
      receiverId_ = data[ReceiverIdKey].toString();
      dateTime_ = QDateTime::fromMSecsSinceEpoch(data[DateTimeKey].toDouble());

      QByteArray local_nonce = QByteArray::fromBase64(data[Nonce].toString().toLocal8Bit());

      nonce_.assign(local_nonce.begin(), local_nonce.end());

      rawType_ = RawMessageDataType::Undefined;
   }

   MessageData::MessageData(const MessageData& source, const QJsonObject& jsonData)
      : MessageData(source)
   {
      rawType_ = RawMessageDataType::TextMessage;
      encryptionType_ = EncryptionType::Unencrypted;
      displayText_ = jsonData[QLatin1String("message_text")].toString();
   }

   MessageData::MessageData(const QString &sender, const QString &receiver
                            , const QString &id, const QDateTime &dateTime
                            , const QString&  messagePayload
                            , RawMessageDataType rawType
                            , int state)
      : DataObject(DataObject::Type::MessageData)
      , id_(id)
      , senderId_(sender)
      , receiverId_(receiver)
      , dateTime_(dateTime)
      , state_{state}
      , encryptionType_{EncryptionType::Unencrypted}
      , displayText_{}
      , messagePayload_{messagePayload}
      , rawType_{rawType}
   {

   }

   MessageData::MessageData(const MessageData& source, RawMessageDataType rawType)
      : MessageData(source)
   {
      encryptionType_ = EncryptionType::Unencrypted;
      rawType_ = rawType;
   }

   MessageData::MessageData(const QString &sender, const QString &receiver
                  , const QString &id, const QDateTime &dateTime
                  , const QString& messageText, int state)
      : DataObject(DataObject::Type::MessageData)
      , id_(id)
      , senderId_(sender)
      , receiverId_(receiver)
      , dateTime_(dateTime)
      , state_{state}
      , encryptionType_{EncryptionType::Unencrypted}
      , displayText_{messageText}
      , rawType_{RawMessageDataType::TextMessage}
   {
      messagePayload_ = serializePayload();
   }

   MessageData::MessageData(const MessageData& source
               , const MessageData::EncryptionType &type
               , const QString& encryptedPayload)
      : MessageData(source)
   {
      encryptionType_ = type;
      displayText_ = QString{};
      messagePayload_ = encryptedPayload;
      rawType_ = RawMessageDataType::TextMessage;
   }

   MessageData::MessageData(const MessageData& source)
      : DataObject(DataObject::Type::MessageData)
      , id_{source.id_}
      , senderId_{source.senderId_}
      , receiverId_{source.receiverId_}
      , dateTime_{source.dateTime_}
      , state_{source.state_}
      , nonce_{source.nonce_}
      , direction_{source.direction_}
      , encryptionType_{source.encryptionType_}
      , displayText_{source.displayText_}
      , messagePayload_{source.messagePayload_}
      , rawType_{source.rawType_}
      , loadedFromHistory_{source.loadedFromHistory_}
   {}

   MessageData::RawMessageDataType MessageData::messageDataType() const
   {
      return rawType_;
   }

   MessageData::MessageDirection MessageData::messageDirectoin() const
   {
      return direction_;
   }

   void MessageData::messageDirectionUpdate()
   {}

   void MessageData::setMessageDirection(MessageDirection direction)
   {
      direction_ = direction;
      messageDirectionUpdate();
   }

   QJsonObject MessageData::toJson() const
   {
      QJsonObject data = DataObject::toJson();

      data[SenderIdKey] = senderId_;
      data[ReceiverIdKey] = receiverId_;
      data[DateTimeKey] = dateTime_.toMSecsSinceEpoch();
      data[MessageKey] = messagePayload();
      data[StatusKey] = state_;
      data[MessageIdKey] = id_;

      data[Nonce] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(nonce_.data()), int(nonce_.size())).toBase64());
      data[EncryptionTypeKey] = static_cast<int>(encryptionType());
      return data;
   }

   std::string MessageData::jsonAssociatedData() const
   {
      QJsonObject data = DataObject::toJson();

      data[SenderIdKey] = senderId_;
      data[ReceiverIdKey] = receiverId_;
      data[Nonce] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(nonce_.data()), int(nonce_.size())).toBase64());

      QJsonDocument jsonDocument(data);
      return QString::fromLatin1(jsonDocument.toJson(QJsonDocument::Compact)).toStdString();
   }

   std::shared_ptr<MessageData> MessageData::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

      MessageData result{data};

      const auto encryptionType = static_cast<EncryptionType>(data[EncryptionTypeKey].toInt());
      const auto payload = data[MessageKey].toString();

      if (encryptionType == EncryptionType::Unencrypted) {
         return result.CreateDecryptedMessage(payload);
      }

      return result.CreateEncryptedMessage(encryptionType, payload);
   }

   void MessageData::setFlag(const State state)
   {
      state_ |= (int)state;
   }

   void MessageData::unsetFlag(const MessageData::State state)
   {
      state_ &= ~(int)state;
   }

   bool MessageData::testFlag(const MessageData::State stateFlag)
   {
      return state_ & static_cast<int>(stateFlag);
   }

   void MessageData::updateState(const int newState)
   {
      state_ = newState;
   }

   QString MessageData::setId(const QString& id)
   {
      QString oldId = id_;
      id_ = id;
      return oldId;
   }

   void MessageData::setNonce(const Botan::SecureVector<uint8_t> &nonce)
   {
      nonce_ = nonce;
   }

   Botan::SecureVector<uint8_t> MessageData::nonce() const
   {
      return nonce_;
   }

   size_t MessageData::defaultNonceSize() const
   {
      return NONCE_SIZE;
   }

   MessageData::EncryptionType MessageData::encryptionType() const
   {
      return encryptionType_;
   }

   std::shared_ptr<MessageData> MessageData::CreateEncryptedMessage(const MessageData::EncryptionType &type, const QString& messagePayload)
   {
      if (type == EncryptionType::Unencrypted) {
         // there is another method for this
         return CreateDecryptedMessage(messagePayload);
      }

      return std::make_shared<MessageData>(*this, type, messagePayload);
   }

   std::shared_ptr<MessageData> MessageData::CreateDecryptedMessage(const QString& messagePayload)
   {
      messagePayload_ = messagePayload;

      QJsonParseError error;
      QJsonDocument doc = QJsonDocument::fromJson(messagePayload.toUtf8(), &error);

      if (error.error != QJsonParseError::NoError) {
         return nullptr;
      }

      QJsonObject jsonObject = doc.object();
      RawMessageDataType rawType = static_cast<RawMessageDataType>(jsonObject[QString::fromLatin1("raw_message_type")].toInt());

      switch (rawType) {
      case RawMessageDataType::TextMessage:
         return std::make_shared<MessageData>(*this, jsonObject);
      case RawMessageDataType::OTCReqeust:
         return std::make_shared<OTCRequestData>(*this, jsonObject);
      case RawMessageDataType::OTCResponse:
         return std::make_shared<OTCResponseData>(*this, jsonObject);
      case RawMessageDataType::OTCUpdate:
         return std::make_shared<OTCUpdateData>(*this, jsonObject);
      case RawMessageDataType::OTCCloseTrading:
         return std::make_shared<OTCCloseTradingData>(*this, jsonObject);
      default:
         return nullptr;
      }
   }

   QString MessageData::displayText() const
   {
      return displayText_;
   }

   QString MessageData::messagePayload() const
   {
      return messagePayload_;
   }

   void MessageData::updatePayload(const QString& payload)
   {
      messagePayload_ = payload;
   }

   bool MessageData::loadedFromHistory() const
   {
      return loadedFromHistory_;
   }

   void MessageData::setLoadedFromHistory()
   {
      loadedFromHistory_ = true;
   }
}

