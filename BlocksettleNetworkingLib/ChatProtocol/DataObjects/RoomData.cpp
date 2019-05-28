#include "RoomData.h"
#include "../ProtocolDefinitions.h"
using namespace Chat;

RoomData::RoomData(const QString& roomId, const QString& ownerId, const QString& roomTitle, const QString& roomKey, bool isPrivate, bool sendUserUpdates, bool displayUserList, bool displayTrayNotification)
   : DataObject (DataObject::Type::RoomData)
   , id_(roomId)
   , ownerId_(ownerId)
   , title_(roomTitle)
   , roomKey_(roomKey)
   , isPrivate_(isPrivate)
   , sendUserUpdates_(sendUserUpdates)
   , displayUserList_(displayUserList)
   , displayTrayNotification_(displayTrayNotification)
   , haveNewMessage_(false)
{}

QString RoomData::getId() {return id_;}

QString RoomData::getOwnerId() {return ownerId_;}

QString RoomData::getTitle() {return title_;}

QString RoomData::getRoomKey() {return roomKey_; }

bool RoomData::isPrivate() {return isPrivate_;}

bool RoomData::sendUserUpdates() {return sendUserUpdates_;}

bool RoomData::displayUserList() {return displayUserList_;}

bool RoomData::haveNewMessage() const
{
   return haveNewMessage_;
}

void RoomData::setHaveNewMessage(bool haveNewMessage)
{
   haveNewMessage_ = haveNewMessage;
}

bool RoomData::displayTrayNotification() const
{
   return displayTrayNotification_;
}

void RoomData::setDisplayTrayNotification(const bool &displayTrayNotification)
{
   displayTrayNotification_ = displayTrayNotification;
}

QJsonObject RoomData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   data[IdKey] = id_;
   data[RoomOwnerIdKey] = ownerId_;
   data[RoomTitleKey] = title_;
   data[RoomKeyKey] = roomKey_;
   data[RoomIsPrivateKey] = isPrivate_;
   data[RoomSendUserUpdatesKey] = sendUserUpdates_;
   data[RoomDisplayUserListKey] = displayUserList_;
   data[RoomDisplayTrayNotificationKey] = displayTrayNotification_;

   return data;
}

std::shared_ptr<RoomData> RoomData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString id = data[IdKey].toString();
   QString ownerId = data[RoomOwnerIdKey].toString();
   QString title = data[RoomTitleKey].toString();
   QString roomKey = data[RoomKeyKey].toString();
   bool isPrivate = data[RoomIsPrivateKey].toBool();
   bool sendUserUpdates = data[RoomSendUserUpdatesKey].toBool();
   bool displayUserList = data[RoomDisplayUserListKey].toBool();
   bool displayTrayNotification = data[RoomDisplayTrayNotificationKey].toBool();

   return std::make_shared<RoomData>(id, ownerId, title, roomKey, isPrivate, sendUserUpdates, displayUserList, displayTrayNotification);
}
