#include "ContactRecordData.h"

using namespace Chat;

ContactRecordData::ContactRecordData(const QString &userId, const QString &contactId,
   ContactStatus status, BinaryData publicKey, const QString& displayName)
   : DataObject (DataObject::Type::ContactRecordData),
   userId_(userId),
   contactId_(contactId),
   status_(status),
   publicKey_(publicKey),
   displayName_(displayName)
{

}

QString ContactRecordData::getUserId()
{
   return userId_;
}

QString ContactRecordData::getContactId()
{
   return contactId_;
}

ContactStatus ContactRecordData::getContactStatus()
{
   return status_;
}

BinaryData ContactRecordData::getContactPublicKey()
{
   return publicKey_;
}

QJsonObject Chat::ContactRecordData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   data[UserIdKey] = userId_;
   data[DisplayNameKey] = displayName_;
   data[ContactIdKey] = contactId_;
   data[ContactStatusKey] = static_cast<int>(status_);
   data[PublicKeyKey] = QString::fromStdString(publicKey_.toHexStr());

   return data;
}

std::shared_ptr<ContactRecordData> ContactRecordData::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString userId = data[UserIdKey].toString();
   QString contactId = data[ContactIdKey].toString();
   ContactStatus status = static_cast<ContactStatus>(data[ContactStatusKey].toInt());
   BinaryData publicKey = BinaryData::CreateFromHex(data[PublicKeyKey].toString().toStdString());
   QString displayName = data[DisplayNameKey].toString();

   return std::make_shared<ContactRecordData>(userId, contactId, status, publicKey, displayName);
}

void ContactRecordData::setContactStatus(const ContactStatus &status)
{
   status_ = status;
}

QString ContactRecordData::getDisplayName() const
{
   return displayName_;
}

void ContactRecordData::setDisplayName(const QString &displayName)
{
   displayName_ = displayName;
}

bool ContactRecordData::hasDisplayName() const
{
   return !getDisplayName().isEmpty();
}

void ContactRecordData::setUserId(const QString &userId)
{
   userId_ = userId;
}

bool ContactRecordData::isValid() const
{
   return !userId_.isEmpty();
}
