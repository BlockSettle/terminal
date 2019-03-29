#include "ContactRecordData.h"

using namespace Chat;

ContactRecordData::ContactRecordData(const QString &userId, const QString &contactId, ContactStatus status, autheid::PublicKey publicKey)
   :DataObject (DataObject::Type::ContactRecordData)
   , userId_(userId)
   , contactId_(contactId)
   , status_(status)
   , publicKey_(publicKey)
{

}

QString ContactRecordData::getContactForId()
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

autheid::PublicKey ContactRecordData::getContactPublicKey()
{
   return publicKey_;
}

QJsonObject Chat::ContactRecordData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   data[UserIdKey] = userId_;
   data[ContactIdKey] = contactId_;
   data[ContactStatusKey] = static_cast<int>(status_);
   data[PublicKeyKey] = QString::fromStdString(publicKeyToString(publicKey_));

   return data;
}

std::shared_ptr<ContactRecordData> ContactRecordData::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString userId = data[UserIdKey].toString();
   QString contactId = data[ContactIdKey].toString();
   ContactStatus status = static_cast<ContactStatus>(data[ContactStatusKey].toInt());
   autheid::PublicKey publicKey = publicKeyFromString(data[PublicKeyKey].toString().toStdString());

   return std::make_shared<ContactRecordData>(userId, contactId, status, publicKey);
}
