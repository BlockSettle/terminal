#include "MessageData.h"

#include "../ProtocolDefinitions.h"

namespace Chat {
   MessageData::MessageData(const QString& senderId, const QString& receiverId
         , const QString &id, const QDateTime& dateTime
         , const QString& messageData, int state)
      : DataObject(DataObject::Type::MessageData)
      , id_(id), senderId_(senderId), receiverId_(receiverId)
      , dateTime_(dateTime)
      , messageData_(messageData), state_(state)
   {
   }
   
   QJsonObject MessageData::toJson() const
   {
      QJsonObject data = DataObject::toJson();
   
      data[SenderIdKey] = senderId_;
      data[ReceiverIdKey] = receiverId_;
      data[DateTimeKey] = dateTime_.toMSecsSinceEpoch();
      data[MessageKey] = messageData_;
      data[StatusKey] = state_;
      data[MessageIdKey] = id_;
   
      return data;
   }
   
//   std::string MessageData::toJsonString() const
//   {
//      return serializeData(this);
//   }
   
   std::shared_ptr<MessageData> MessageData::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   
      QString senderId = data[SenderIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      QDateTime dtm = QDateTime::fromMSecsSinceEpoch(data[DateTimeKey].toDouble());
      QString messageData = data[MessageKey].toString();
      QString id =  data[MessageIdKey].toString();
      const int state = data[StatusKey].toInt();
   
      return std::make_shared<MessageData>(senderId, receiverId, id, dtm, messageData, state);
   }
   
   void MessageData::setFlag(const State state)
   {
      state_ |= (int)state;
   }
   
   void MessageData::unsetFlag(const MessageData::State state)
   {
      state_ &= ~(int)state;
   }

   void MessageData::updateState(const int newState)
   {
      int mask = ~static_cast<int>(Chat::MessageData::State::Encrypted);
      int set = mask & newState;
      int unset = ~(set ^ mask);
      state_ = (state_ & unset) | set;
   }
   
   bool MessageData::decrypt(const autheid::PrivateKey& privKey)
   {
      if (!(state_ & (int)State::Encrypted)) {
         return false;
      }
      const auto message_bytes = QByteArray::fromBase64(messageData_.toUtf8());
      const auto decryptedData = autheid::decryptData(
         message_bytes.data(), message_bytes.size(), privKey);
      messageData_ = QString::fromUtf8((char*)decryptedData.data(), decryptedData.size());
      state_ &= ~(int)State::Encrypted;
      return true;
   }
   
   bool MessageData::encrypt(const autheid::PublicKey& pubKey)
   {
      if (state_ & (int)State::Encrypted) {
         return false;
      }
      const QByteArray message_bytes = messageData_.toUtf8();
      auto data = autheid::encryptData(message_bytes.data(), size_t(message_bytes.size()), pubKey);
      messageData_ = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(data.data()), int(data.size())).toBase64());
      state_ |= (int)State::Encrypted;
      return true;
   }
   
   QString MessageData::setId(const QString& id)
   {
      QString oldId = id_;
      id_ = id;
      return oldId;
   }
}
