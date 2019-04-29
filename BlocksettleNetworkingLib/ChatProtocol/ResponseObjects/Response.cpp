#include "Response.h"
#include <map>
#include <memory>

using namespace Chat;

#include "HeartbeatPongResponse.h"
#include "UsersListResponse.h"
#include "MessagesResponse.h"
#include "AskForPublicKeyResponse.h"
#include "SendOwnPublicKeyResponse.h"
#include "LoginResponse.h"
#include "SendMessageResponse.h"
#include "MessageChangeStatusResponse.h"
#include "ContactsActionResponseDirect.h"
#include "ContactsActionResponseServer.h"
#include "ChatroomsListResponse.h"
#include "PendingMessagesResponse.h"
#include "RoomMessagesResponse.h"
#include "ContactsListResponse.h"
#include "SearchUsersResponse.h"
#include "LogoutResponse.h"

static std::map<std::string, ResponseType> ResponseTypeFromString
{
       { "ResponseError"               ,   ResponseType::ResponseError               }
   ,   { "ResponseHeartbeatPong"       ,   ResponseType::ResponseHeartbeatPong       }
   ,   { "ResponseLogin"               ,   ResponseType::ResponseLogin               }
   ,   { "ResponseMessages"            ,   ResponseType::ResponseMessages            }
   ,   { "ResponseSuccess"             ,   ResponseType::ResponseSuccess             }
   ,   { "ResponseUsersList"           ,   ResponseType::ResponseUsersList           }
   ,   { "ResponseAskForPublicKey"     ,   ResponseType::ResponseAskForPublicKey     }
   ,   { "ResponseSendOwnPublicKey"    ,   ResponseType::ResponseSendOwnPublicKey    }
   ,   { "ResponsePendingMessage"      ,   ResponseType::ResponsePendingMessage      }
   ,   { "ResponseSendMessage"         ,   ResponseType::ResponseSendMessage         }
   ,   { "ResponseChangeMessageStatus" ,   ResponseType::ResponseChangeMessageStatus }
   ,   { "ResponseContactsActionDirect",   ResponseType::ResponseContactsActionDirect}
   ,   { "ResponseContactsActionServer",   ResponseType::ResponseContactsActionServer}
   ,   { "ResponseChatroomsList"       ,   ResponseType::ResponseChatroomsList       }
   ,   { "ResponseRoomMessages"        ,   ResponseType::ResponseRoomMessages        }
   ,   { "ResponseContactsList"        ,   ResponseType::ResponseContactsList        }
   ,   { "ResponseSearchUsers"         ,   ResponseType::ResponseSearchUsers         }
   ,   { "ResponseLogout"              ,   ResponseType::ResponseLogout              }
};


static std::map<ResponseType, std::string> ResponseTypeToString
{
       { ResponseType::ResponseError               ,  "ResponseError"               }
   ,   { ResponseType::ResponseHeartbeatPong       ,  "ResponseHeartbeatPong"       }
   ,   { ResponseType::ResponseLogin               ,  "ResponseLogin"               }
   ,   { ResponseType::ResponseMessages            ,  "ResponseMessages"            }
   ,   { ResponseType::ResponseSuccess             ,  "ResponseSuccess"             }
   ,   { ResponseType::ResponseUsersList           ,  "ResponseUsersList"           }
   ,   { ResponseType::ResponseAskForPublicKey     ,  "ResponseAskForPublicKey"     }
   ,   { ResponseType::ResponseSendOwnPublicKey    ,  "ResponseSendOwnPublicKey"    }
   ,   { ResponseType::ResponsePendingMessage      ,  "ResponsePendingMessage"      }
   ,   { ResponseType::ResponseSendMessage         ,  "ResponseSendMessage"         }
   ,   { ResponseType::ResponseChangeMessageStatus ,  "ResponseChangeMessageStatus" }
   ,   { ResponseType::ResponseContactsActionDirect,  "ResponseContactsActionDirect"}
   ,   { ResponseType::ResponseContactsActionServer,  "ResponseContactsActionServer"}
   ,   { ResponseType::ResponseChatroomsList       ,  "ResponseChatroomsList"       }
   ,   { ResponseType::ResponseRoomMessages        ,  "ResponseRoomMessages"        }
   ,   { ResponseType::ResponseContactsList        ,  "ResponseContactsList"        }
   ,   { ResponseType::ResponseSearchUsers         ,  "ResponseSearchUsers"         }
   ,   { ResponseType::ResponseLogout              ,  "ResponseLogout"              }
};

template <typename T>
QJsonObject Message<T>::toJson() const
{
   QJsonObject data;

   data[VersionKey] = QString::fromStdString(version_);

   return data;
}

QJsonObject Response::toJson() const
{
   QJsonObject data = Message<ResponseType>::toJson();

   data[TypeKey] = QString::fromStdString(ResponseTypeToString[messageType_]);

   return data;
}

std::string Response::getData() const
{
   return serializeData(this);
}

std::shared_ptr<Response> Response::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   const ResponseType responseType = ResponseTypeFromString[data[TypeKey].toString().toStdString()];

   switch (responseType)
   {
      case ResponseType::ResponseHeartbeatPong:
         return std::make_shared<HeartbeatPongResponse>();

      case ResponseType::ResponseUsersList:
         return UsersListResponse::fromJSON(jsonData);

      case ResponseType::ResponseMessages:
         return MessagesResponse::fromJSON(jsonData);

      case ResponseType::ResponseLogin:
         return LoginResponse::fromJSON(jsonData);

      case ResponseType::ResponseAskForPublicKey:
         return AskForPublicKeyResponse::fromJSON(jsonData);

      case ResponseType::ResponseSendOwnPublicKey:
         return SendOwnPublicKeyResponse::fromJSON(jsonData);
      
     case ResponseType::ResponsePendingMessage:
        return PendingMessagesResponse::fromJSON(jsonData);
        
      case ResponseType::ResponseSendMessage:
         return SendMessageResponse::fromJSON(jsonData);
         
      case ResponseType::ResponseChangeMessageStatus:
         return MessageChangeStatusResponse::fromJSON(jsonData);
      
      case ResponseType::ResponseContactsActionDirect:
         return ContactsActionResponseDirect::fromJSON(jsonData);

      case ResponseType::ResponseContactsActionServer:
         return ContactsActionResponseServer::fromJSON(jsonData);
      
      case ResponseType::ResponseChatroomsList:
         return ChatroomsListResponse::fromJSON(jsonData);
      case ResponseType::ResponseRoomMessages:
         return RoomMessagesResponse::fromJSON(jsonData);

      case ResponseType::ResponseContactsList:
         return ContactsListResponse::fromJSON(jsonData);

      case ResponseType::ResponseSearchUsers:
         return SearchUsersResponse::fromJSON(jsonData);

      case ResponseType::ResponseLogout:
         return std::make_shared<LogoutResponse>();

      default:
         break;
   }

   return nullptr;
}

PendingResponse::PendingResponse(ResponseType type, const QString& id)
   : Response(type), id_(id)
{
   
}

QJsonObject PendingResponse::toJson() const
{
   QJsonObject data = Response::toJson();
   return data;
}

QString Chat::PendingResponse::getId() const
{
   return id_;
}

void Chat::PendingResponse::setId(const QString& id)
{
   id_ = id;
}

void Chat::PendingResponse::handle(ResponseHandler &)
{
   return;
}
