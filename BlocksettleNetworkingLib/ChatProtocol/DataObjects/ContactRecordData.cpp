#include "ContactRecordData.h"

namespace Chat {

   ContactRecordData::ContactRecordData(const QString& userId, const QString& contactId,
      const ContactStatus& status, const QString& publicKey, const QDateTime& publicKeyTime, const QString& displayName)
      : DataObject(DataObject::Type::ContactRecordData),
      userId_(userId),
      contactId_(contactId),
      status_(status),
      publicKey_(publicKey),
      publicKeyTime_(publicKeyTime),
      displayName_(displayName)
   {

   }

   QString ContactRecordData::getUserId() const
   {
      return userId_;
   }

   QString ContactRecordData::getContactId() const
   {
      return contactId_;
   }

   ContactStatus ContactRecordData::getContactStatus() const
   {
      return status_;
   }

   QString ContactRecordData::getContactPublicKey() const
   {
      return publicKey_;
   }

   BinaryData ContactRecordData::getContactPublicKeyBinaryData() const
   {
      return BinaryData::CreateFromHex(publicKey_.toStdString());
   }


   QJsonObject Chat::ContactRecordData::toJson() const
   {
      QJsonObject data = DataObject::toJson();

      data[UserIdKey] = userId_;
      data[DisplayNameKey] = displayName_;
      data[ContactIdKey] = contactId_;
      data[ContactStatusKey] = static_cast<int>(status_);
      data[PublicKeyKey] = publicKey_;
      data[PublicKeyTimeKey] = publicKeyTime_.toMSecsSinceEpoch();

      return data;
   }

   std::shared_ptr<ContactRecordData> ContactRecordData::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

      QString userId = data[UserIdKey].toString();
      QString contactId = data[ContactIdKey].toString();
      ContactStatus status = static_cast<ContactStatus>(data[ContactStatusKey].toInt());
      QString publicKey = data[PublicKeyKey].toString();
      QString displayName = data[DisplayNameKey].toString();
      QDateTime publicKeyTime = QDateTime::fromMSecsSinceEpoch(data[PublicKeyTimeKey].toDouble());

      return std::make_shared<ContactRecordData>(userId, contactId, status, publicKey, publicKeyTime, displayName);
   }

   void ContactRecordData::setContactStatus(const ContactStatus& status)
   {
      status_ = status;
   }

   QString ContactRecordData::getDisplayName() const
   {
      return displayName_;
   }

   void ContactRecordData::setDisplayName(const QString& displayName)
   {
      displayName_ = displayName;
   }

   bool ContactRecordData::hasDisplayName() const
   {
      return !getDisplayName().isEmpty();
   }

   void ContactRecordData::setUserId(const QString& userId)
   {
      userId_ = userId;
   }

   bool ContactRecordData::isValid() const
   {
      return !userId_.isEmpty();
   }

   QDateTime ContactRecordData::getContactPublicKeyTime() const
   {
      return publicKeyTime_;
   }

   void ContactRecordData::contactPublicKeyTime(const QDateTime& publicKeyTime)
   {
      publicKeyTime_ = publicKeyTime;
   }

}
