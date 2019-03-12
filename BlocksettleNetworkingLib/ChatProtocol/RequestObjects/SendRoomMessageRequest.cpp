#include "SendRoomMessageRequest.h"

namespace Chat {

   
   SendRoomMessageRequest::SendRoomMessageRequest(const std::string& clientId
   , const std::string& roomId
   , const std::string& messageData)
   : Request (RequestType::RequestSendRoomMessage, clientId)
   ,roomId_(roomId)
   , messageData_(messageData)
   {
      
   }
   
   QJsonObject SendRoomMessageRequest::toJson() const
   {
       QJsonObject data = Request::toJson();
       data[RoomKey] = QString::fromStdString(roomId_);
       data[MessageKey] = QString::fromStdString(messageData_);
       return data;
   }
   
   std::shared_ptr<Chat::Request> SendRoomMessageRequest::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
       QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
       return std::make_shared<SendRoomMessageRequest>(
                         clientId
                        , data[RoomKey].toString().toStdString()
                        , data[MessageKey].toString().toStdString());
   }
   
   void SendRoomMessageRequest::handle(RequestHandler& handler)
   {
       handler.OnSendRoomMessage(*this);
   }
   
   const std::string& SendRoomMessageRequest::getMessageData() const { return messageData_; }
   
   const std::string& SendRoomMessageRequest::getRoomId() const { return roomId_; }
}
