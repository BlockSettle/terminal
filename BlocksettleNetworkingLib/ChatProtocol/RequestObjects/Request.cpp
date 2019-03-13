#include "Request.h"
#include <map>


#include "HeartbeatPingRequest.h"
#include "LoginRequest.h"
#include "SendMessageRequest.h"
#include "AskForPublicKeyRequest.h"
#include "SendOwnPublicKeyRequest.h"
#include "OnlineUsersRequest.h"
#include "MessagesRequest.h"
#include "MessageChangeStatusRequest.h"
#include "ContactActionRequest.h"
#include "ChatroomsListRequest.h"
#include "SendRoomMessageRequest.h"

using namespace Chat;

static std::map<std::string, RequestType> RequestTypeFromString
{
       { "RequestHeartbeatPing"      ,   RequestType::RequestHeartbeatPing      }
   ,   { "RequestLogin"              ,   RequestType::RequestLogin              }
   ,   { "RequestLogout"             ,   RequestType::RequestLogout             }
   ,   { "RequestMessages"           ,   RequestType::RequestMessages           }
   ,   { "RequestSendMessage"        ,   RequestType::RequestSendMessage        }
   ,   { "RequestOnlineUsers"        ,   RequestType::RequestOnlineUsers        }
   ,   { "RequestAskForPublicKey"    ,   RequestType::RequestAskForPublicKey    }
   ,   { "RequestSendOwnPublicKey"   ,   RequestType::RequestSendOwnPublicKey   }
   ,   { "RequestChangeMessageStatus",   RequestType::RequestChangeMessageStatus}
   ,   { "RequestContactsAction"     ,   RequestType::RequestContactsAction     }
   ,   { "RequestChatroomsList"      ,   RequestType::RequestChatroomsList      }
   ,   { "RequestSendRoomMessage"    ,   RequestType::RequestSendRoomMessage    }
};


static std::map<RequestType, std::string> RequestTypeToString
{
       { RequestType::RequestHeartbeatPing      ,   "RequestHeartbeatPing"      }
   ,   { RequestType::RequestLogin              ,   "RequestLogin"              }
   ,   { RequestType::RequestLogout             ,   "RequestLogout"             }
   ,   { RequestType::RequestMessages           ,   "RequestMessages"           }
   ,   { RequestType::RequestSendMessage        ,   "RequestSendMessage"        }
   ,   { RequestType::RequestOnlineUsers        ,   "RequestOnlineUsers"        }
   ,   { RequestType::RequestAskForPublicKey    ,   "RequestAskForPublicKey"    }
   ,   { RequestType::RequestSendOwnPublicKey   ,   "RequestSendOwnPublicKey"   }
   ,   { RequestType::RequestChangeMessageStatus,   "RequestChangeMessageStatus"}
   ,   { RequestType::RequestContactsAction     ,   "RequestContactsAction"     }
   ,   { RequestType::RequestChatroomsList      ,   "RequestChatroomsList"      }
   ,   { RequestType::RequestSendRoomMessage    ,   "RequestSendRoomMessage"    }
};

template <typename T>
QJsonObject Message<T>::toJson() const
{
   QJsonObject data;

   data[VersionKey] = QString::fromStdString(version_);

   return data;
}

Request::Request(RequestType requestType, const std::string& clientId)
   : Message<RequestType> (requestType)
   , clientId_(clientId)
{
}

std::string Request::getClientId() const { return clientId_; }

std::shared_ptr<Request> Request::fromJSON(const std::string& clientId, const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   const RequestType requestType = RequestTypeFromString[data[TypeKey].toString().toStdString()];

   switch (requestType)
   {
      case RequestType::RequestHeartbeatPing:
         return std::make_shared<HeartbeatPingRequest>(clientId);

      case RequestType::RequestLogin:
        return std::make_shared<LoginRequest>(clientId
                 , data[AuthIdKey].toString().toStdString()
                 , data[JwtKey].toString().toStdString());

      case RequestType::RequestSendMessage:
         return SendMessageRequest::fromJSON(clientId, jsonData);

      case RequestType::RequestOnlineUsers:
         return std::make_shared<OnlineUsersRequest>(clientId
                 , data[AuthIdKey].toString().toStdString());

      case RequestType::RequestMessages:
         return std::make_shared<MessagesRequest>(clientId
                 , data[SenderIdKey].toString().toStdString()
                 , data[ReceiverIdKey].toString().toStdString());

      case RequestType::RequestLogout:
         return std::make_shared<LogoutRequest>(clientId
                 , data[AuthIdKey].toString().toStdString()
                 , data[JwtKey].toString().toStdString());

      case RequestType::RequestAskForPublicKey:
         return std::make_shared<AskForPublicKeyRequest>(clientId,
               data[SenderIdKey].toString().toStdString(),
               data[ReceiverIdKey].toString().toStdString());

      case RequestType::RequestSendOwnPublicKey:
         return std::make_shared<SendOwnPublicKeyRequest>(clientId
            , data[ReceiverIdKey].toString().toStdString()
            , data[SenderIdKey].toString().toStdString()
            , publicKeyFromString(data[PublicKeyKey].toString().toStdString()));

      case RequestType::RequestChangeMessageStatus:
         return MessageChangeStatusRequest::fromJSON(clientId, jsonData);

      case RequestType::RequestContactsAction:
         return ContactActionRequest::fromJSON(clientId, jsonData);

      case RequestType::RequestChatroomsList:
         return std::make_shared<ChatroomsListRequest>(clientId
               , data[SenderIdKey].toString().toStdString());
      case RequestType::RequestSendRoomMessage:
         return SendRoomMessageRequest::fromJSON(clientId, jsonData);

      default:
         break;
   }

   return nullptr;
}

std::string Request::getData() const
{
   return serializeData(this);
}

QJsonObject Request::toJson() const
{
   QJsonObject data = Message<RequestType>::toJson();

   data[TypeKey] = QString::fromStdString(RequestTypeToString[messageType_]);

   return data;
}
