#include "MessageData.h"

#include "../ProtocolDefinitions.h"

#include <QDebug>

namespace Chat {

   const size_t NONCE_SIZE = 24;

   MessageData::MessageData(const QString& senderId, const QString& receiverId, const QString &id, const QDateTime& dateTime,
      const QString& messageData, int state)
      : DataObject(DataObject::Type::MessageData),
      id_(id),
      senderId_(senderId),
      receiverId_(receiverId),
      dateTime_(dateTime),
      messageData_(messageData),
      state_(state),
      encryptionType_(EncryptionType::Unencrypted)
   {
   }

   void MessageData::setMessageData(const QString& messageData)
   {
      messageData_ = messageData;
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

      QString senderId = data[SenderIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      QDateTime dtm = QDateTime::fromMSecsSinceEpoch(data[DateTimeKey].toDouble());
      QString messageData = data[MessageKey].toString();
      QString id =  data[MessageIdKey].toString();
      const int state = data[StatusKey].toInt();
      QByteArray local_nonce = QByteArray::fromBase64(data[Nonce].toString().toLocal8Bit());
      Botan::SecureVector<uint8_t> nonce(local_nonce.begin(), local_nonce.end());

      std::shared_ptr<MessageData> msg = std::make_shared<MessageData>(senderId, receiverId, id, dtm, messageData, state);
      msg->setNonce(nonce);
      msg->setEncryptionType(static_cast<EncryptionType>(data[EncryptionTypeKey].toInt()));
      return msg;
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

   void MessageData::setEncryptionType(const MessageData::EncryptionType &type)
   {
      encryptionType_ = type;
   }
}
