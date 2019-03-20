#include "ChatRoomData.h"
#include "../ProtocolDefinitions.h"
using namespace Chat;

ChatRoomData::ChatRoomData(const QString& roomId, const QString& ownerId, const QString& roomTitle, const QString& roomKey, bool isPrivate, bool sendUserUpdates, bool displayUserList)
   : DataObject (DataObject::Type::ChatRoomData)
   , id_(roomId)
   , ownerId_(ownerId)
   , title_(roomTitle)
   , roomKey_(roomKey)
   , isPrivate_(isPrivate)
   , sendUserUpdates_(sendUserUpdates)
   , displayUserList_(displayUserList)
   , haveNewMessage_(false)
{}

QString ChatRoomData::getId() {return id_;}

QString ChatRoomData::getOwnerId() {return ownerId_;}

QString ChatRoomData::getTitle() {return title_;}

QString ChatRoomData::getRoomKey() {return roomKey_; }

bool ChatRoomData::isPrivate() {return isPrivate_;}

bool ChatRoomData::sendUserUpdates() {return sendUserUpdates_;}

bool ChatRoomData::displayUserList() {return displayUserList_;}

bool ChatRoomData::haveNewMessage() const
{
   return haveNewMessage_;
}

void ChatRoomData::setHaveNewMessage(bool haveNewMessage)
{
   haveNewMessage_ = haveNewMessage;
}

QJsonObject ChatRoomData::toJson() const
{
   QJsonObject data = DataObject::toJson();
   
   data[IdKey] = id_;
   data[RoomOwnerIdKey] = ownerId_;
   data[RoomTitleKey] = title_;
   data[RoomKeyKey] = roomKey_;
   data[RoomIsPrivateKey] = isPrivate_;
   data[RoomSendUserUpdatesKey] = sendUserUpdates_;
   data[RoomDisplayUserListKey] = displayUserList_;
         
   return data;
}

std::shared_ptr<ChatRoomData> ChatRoomData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   
   QString id = data[IdKey].toString();
   QString ownerId = data[RoomOwnerIdKey].toString();
   QString title = data[RoomTitleKey].toString();
   QString roomKey = data[RoomKeyKey].toString();
   bool isPrivate = data[RoomIsPrivateKey].toBool();
   bool sendUserUpdates = data[RoomSendUserUpdatesKey].toBool();
   bool displayUserList = data[RoomDisplayUserListKey].toBool();
   
   return std::make_shared<ChatRoomData>(id, ownerId, title, roomKey, isPrivate, sendUserUpdates, displayUserList);
}
