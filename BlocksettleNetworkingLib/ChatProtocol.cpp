#include "ChatProtocol.h"

#include <sstream>

#include <QStringLiteral>
#include <QDebug>
#include "EncryptionUtils.h"


using namespace Chat;


static const QString VersionKey   = QStringLiteral("version");
static const QString NameKey      = QStringLiteral("name");
static const QString TypeKey      = QStringLiteral("type");
static const QString TextKey      = QStringLiteral("text");
static const QString RoomKey      = QStringLiteral("room");
static const QString RoomsKey     = QStringLiteral("rooms");
static const QString MessageKey   = QStringLiteral("message");
static const QString FromKey      = QStringLiteral("from");
static const QString ContactsKey  = QStringLiteral("fromid");
static const QString IdKey        = QStringLiteral("id");
static const QString AuthIdKey    = QStringLiteral("authid");
static const QString JwtKey       = QStringLiteral("jwt");
static const QString PasswordKey  = QStringLiteral("passwd");
static const QString ReceiverIdKey  = QStringLiteral("toid");
static const QString SenderIdKey  = QStringLiteral("fromid");
static const QString StatusKey    = QStringLiteral("status");
static const QString UsersKey     = QStringLiteral("users");
static const QString DateTimeKey  = QStringLiteral("datetm");
static const QString DataKey      = QStringLiteral("data");
static const QString CommandKey = QStringLiteral("cmd");


static std::map<std::string, RequestType> RequestTypeFromString
{
       { "RequestHeartbeatPing" ,   RequestType::RequestHeartbeatPing  }
   ,   { "RequestLogin"         ,   RequestType::RequestLogin          }
   ,   { "RequestLogout"        ,   RequestType::RequestLogout         }
   ,   { "RequestMessages"      ,   RequestType::RequestMessages       }
   ,   { "RequestSendMessage"   ,   RequestType::RequestSendMessage    }
   ,   { "RequestOnlineUsers"   ,   RequestType::RequestOnlineUsers    }
};


static std::map<RequestType, std::string> RequestTypeToString
{
       { RequestType::RequestHeartbeatPing  ,  "RequestHeartbeatPing" }
   ,   { RequestType::RequestLogin          ,  "RequestLogin"         }
   ,   { RequestType::RequestLogout         ,  "RequestLogout"        }
   ,   { RequestType::RequestMessages       ,  "RequestMessages"      }
   ,   { RequestType::RequestSendMessage    ,  "RequestSendMessage"   }
   ,   { RequestType::RequestOnlineUsers    ,  "RequestOnlineUsers"   }
};


static std::map<std::string, ResponseType> ResponseTypeFromString
{
       { "ResponseError"         ,   ResponseType::ResponseError          }
   ,   { "ResponseHeartbeatPong" ,   ResponseType::ResponseHeartbeatPong  }
   ,   { "ResponseLogin"         ,   ResponseType::ResponseLogin          }
   ,   { "ResponseMessages"      ,   ResponseType::ResponseMessages       }
   ,   { "ResponseSuccess"       ,   ResponseType::ResponseSuccess        }
   ,   { "ResponseUsersList"     ,   ResponseType::ResponseUsersList      }
};


static std::map<ResponseType, std::string> ResponseTypeToString
{
       { ResponseType::ResponseError         ,  "ResponseError"         }
   ,   { ResponseType::ResponseHeartbeatPong ,  "ResponseHeartbeatPong" }
   ,   { ResponseType::ResponseLogin         ,  "ResponseLogin"         }
   ,   { ResponseType::ResponseMessages      ,  "ResponseMessages"      }
   ,   { ResponseType::ResponseSuccess       ,  "ResponseSuccess"       }
   ,   { ResponseType::ResponseUsersList     ,  "ResponseUsersList"     }
};


template <typename T>
std::string serializeData(const T* thisPtr)
{
   auto data = QJsonDocument(thisPtr->toJson());
   QString serializedData = QString::fromUtf8(data.toJson());
   return serializedData.toStdString();
}

template <typename T>
QJsonObject Message<T>::toJson() const
{
   QJsonObject data;

   data[VersionKey] = QString::fromStdString(version_);

   return data;
}


std::shared_ptr<Request> Request::fromJSON(const std::string& clientId, const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   RequestType requestType = RequestTypeFromString[data[TypeKey].toString().toStdString()];

   switch (requestType)
   {
      case RequestType::RequestHeartbeatPing:
         return std::make_shared<HeartbeatPingRequest>(clientId);

      case RequestType::RequestLogin:
        return std::make_shared<LoginRequest>(
                   clientId
                 , data[AuthIdKey].toString().toStdString()
                 , data[JwtKey].toString().toStdString());

      case RequestType::RequestSendMessage:
         return SendMessageRequest::fromJSON(clientId, jsonData);

      case RequestType::RequestOnlineUsers:
         return std::make_shared<OnlineUsersRequest>(
                   clientId
                 , data[AuthIdKey].toString().toStdString());

      case RequestType::RequestMessages:
         return std::make_shared<MessagesRequest>(
                   clientId
                 , data[SenderIdKey].toString().toStdString()
                 , data[ReceiverIdKey].toString().toStdString());

      case RequestType::RequestLogout:
         return std::make_shared<LogoutRequest>(
                   clientId
                 , data[AuthIdKey].toString().toStdString()
                 , data[JwtKey].toString().toStdString());

      default:
         break;
   }

   return std::shared_ptr<Request>();
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

   ResponseType responseType = ResponseTypeFromString[data[TypeKey].toString().toStdString()];

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

      default:
         break;
   }

   return std::shared_ptr<Response>();
}


HeartbeatPingRequest::HeartbeatPingRequest(const std::string& clientId)
   : Request (RequestType::RequestHeartbeatPing, clientId)
{
}

void HeartbeatPingRequest::handle(RequestHandler& handler)
{
   handler.OnHeartbeatPing(*this);
}


HeartbeatPongResponse::HeartbeatPongResponse()
   : Response(ResponseType::ResponseHeartbeatPong)
{
}

void HeartbeatPongResponse::handle(ResponseHandler& handler)
{
   handler.OnHeartbeatPong(*this);
}


ListResponse::ListResponse(ResponseType responseType, std::vector<std::string> dataList)
   : Response(responseType)
   , dataList_(dataList)
{
}

QJsonObject ListResponse::toJson() const
{
   QJsonObject data = Response::toJson();

   QJsonArray listJson;

   std::for_each(dataList_.begin(), dataList_.end(), [&](const std::string& userId){
      listJson << QString::fromStdString(userId);
   });

   data[DataKey] = listJson;

   return data;
}

std::vector<std::string> ListResponse::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   std::vector<std::string> dataList;
   QJsonArray usersArray = data[DataKey].toArray();
   foreach(auto userId, usersArray) {
      dataList.push_back(userId.toString().toStdString());
   }
   return dataList;
}


UsersListResponse::UsersListResponse(std::vector<std::string> dataList, Command cmd)
   : ListResponse(ResponseType::ResponseUsersList, dataList), cmd_(cmd)
{
}

void UsersListResponse::handle(ResponseHandler& handler)
{
   handler.OnUsersList(*this);
}

std::shared_ptr<Response> UsersListResponse::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   const auto cmd = static_cast<Command>(data[CommandKey].toInt());
   return std::make_shared<UsersListResponse>(ListResponse::fromJSON(jsonData), cmd);
}

QJsonObject UsersListResponse::toJson() const
{
   auto data = ListResponse::toJson();
   data[CommandKey] = static_cast<int>(cmd_);

   return data;
}


MessagesResponse::MessagesResponse(std::vector<std::string> dataList)
   : ListResponse (ResponseType::ResponseMessages, dataList)
{
}

std::shared_ptr<Response> MessagesResponse::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<MessagesResponse>(ListResponse::fromJSON(jsonData));
}

void MessagesResponse::handle(ResponseHandler& handler)
{
   handler.OnMessages(*this);
}

BaseLoginRequest::BaseLoginRequest(RequestType requestType
                     , const std::string& clientId
                     , const std::string& authId
                     , const std::string& jwt)
   : Request (requestType, clientId)
   , authId_(authId)
   , jwt_(jwt)
{

}


QJsonObject BaseLoginRequest::toJson() const
{
   QJsonObject data = Request::toJson();

   data[AuthIdKey] = QString::fromStdString(authId_);
   data[JwtKey] = QString::fromStdString(jwt_);

   return data;
}


void LoginRequest::handle(RequestHandler& handler)
{
   handler.OnLogin(*this);
}


void LogoutRequest::handle(RequestHandler& handler)
{
   handler.OnLogout(*this);
}


MessageData::MessageData(const QString& senderId, const QString& receiverId
      , const QString &id, const QDateTime& dateTime
      , const QString& messageData, int state)
   : senderId_(senderId), receiverId_(receiverId)
   , id_(id), dateTime_(dateTime)
   , messageData_(messageData), state_(state)
{
}

QJsonObject MessageData::toJson() const
{
   QJsonObject data;

   data[SenderIdKey] = senderId_;
   data[ReceiverIdKey] = receiverId_;
   data[DateTimeKey] = dateTime_.toMSecsSinceEpoch();
   data[MessageKey] = messageData_;
   data[StatusKey] = state_;

   return data;
}

std::string MessageData::toJsonString() const
{
   return serializeData(this);
}

std::shared_ptr<MessageData> MessageData::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString senderId = data[SenderIdKey].toString();
   QString receiverId = data[ReceiverIdKey].toString();
   QDateTime dtm = QDateTime::fromMSecsSinceEpoch(data[DateTimeKey].toDouble());
   QString messageData = data[MessageKey].toString();
   QString id = QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr());

   return std::make_shared<MessageData>(senderId, receiverId, id, dtm, messageData);
}

void MessageData::setFlag(const State state)
{
   state_ |= (int)state;
}

bool MessageData::decrypt(const SecureBinaryData &privKey)
{
   state_ &= ~(int)State::Encrypted;
   return true;
}

bool MessageData::encrypt(const BinaryData &pubKey)
{
   state_ |= (int)State::Encrypted;
   return true;
}


SendMessageRequest::SendMessageRequest(const std::string& clientId
                              , const std::string& messageData)
   : Request(RequestType::RequestSendMessage, clientId)
   , messageData_(messageData)
{
}

QJsonObject SendMessageRequest::toJson() const
{
   QJsonObject data = Request::toJson();

   data[MessageKey] = QString::fromStdString(messageData_);

   return data;
}


std::shared_ptr<Request> SendMessageRequest::fromJSON(const std::string& clientId, const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<SendMessageRequest>(
                     clientId
                    , data[MessageKey].toString().toStdString());
}

void SendMessageRequest::handle(RequestHandler& handler)
{
   handler.OnSendMessage(*this);
}


OnlineUsersRequest::OnlineUsersRequest(const std::string& clientId
               , const std::string& authId)
   : Request(RequestType::RequestOnlineUsers, clientId)
   , authId_(authId)
{
}

QJsonObject OnlineUsersRequest::toJson() const
{
   QJsonObject data = Request::toJson();

   data[AuthIdKey] = QString::fromStdString(authId_);

   return data;
}

void OnlineUsersRequest::handle(RequestHandler& handler)
{
   handler.OnOnlineUsers(*this);
}


MessagesRequest::MessagesRequest(const std::string& clientId
                         , const std::string& senderId
                         , const std::string& receiverId)
   : Request(RequestType::RequestMessages, clientId)
   , senderId_(senderId)
   , receiverId_(receiverId)
{
}

QJsonObject MessagesRequest::toJson() const
{
   QJsonObject data = Request::toJson();

   data[SenderIdKey] = QString::fromStdString(senderId_);
   data[ReceiverIdKey] = QString::fromStdString(receiverId_);

   return data;
}

void MessagesRequest::handle(RequestHandler& handler)
{
   handler.OnRequestMessages(*this);
}


LoginResponse::LoginResponse(const std::string& userId, Status status)
   : Response (ResponseType::ResponseLogin)
   , userId_(userId)
   , status_(status)
{
}

QJsonObject LoginResponse::toJson() const
{
   QJsonObject data = Response::toJson();

   data[SenderIdKey] = QString::fromStdString(userId_);
   data[StatusKey] = static_cast<int>(status_);

   return data;
}

std::shared_ptr<Response> LoginResponse::fromJSON(const std::string& jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<LoginResponse>(
                   data[SenderIdKey].toString().toStdString()
                 , static_cast<Status>(data[StatusKey].toInt()));
}

void LoginResponse::handle(ResponseHandler& handler)
{
   handler.OnLoginReturned(*this);
}
