#include "ChatProtocol.h"

#include <sstream>


#include <QStringLiteral>


using namespace Chat;


static const QString VersionKey = QStringLiteral("version");
static const QString NameKey = QStringLiteral("name");
static const QString TypeKey = QStringLiteral("type");
static const QString TextKey = QStringLiteral("text");
static const QString RoomKey = QStringLiteral("room");
static const QString RoomsKey = QStringLiteral("rooms");
static const QString MessageKey = QStringLiteral("message");
static const QString FromKey = QStringLiteral("from");
static const QString ContactsKey = QStringLiteral("fromid");
static const QString IdKey = QStringLiteral("id");
static const QString AuthIdKey = QStringLiteral("authid");
static const QString PasswordKey = QStringLiteral("passwd");
static const QString ReceiverIdKey = QStringLiteral("toid");
static const QString SenderIdKey = QStringLiteral("fromid");
static const QString StatusKey = QStringLiteral("status");
static const QString UsersKey = QStringLiteral("users");
static const QString DateTimeKey = QStringLiteral("datetm");


static std::map<std::string, RequestType> RequestTypeFromString
{
        { "RequestHeartbeatPing"    ,   RequestType::RequestHeartbeatPing   }
    ,   { "RequestLogin"            ,   RequestType::RequestLogin           }
    ,   { "RequestLogout"           ,   RequestType::RequestLogout          }
    ,   { "RequestReceiveMessages"  ,   RequestType::RequestReceiveMessages }
    ,   { "RequestSendMessage"      ,   RequestType::RequestSendMessage     }
    ,   { "RequestOnlineUsers"      ,   RequestType::RequestOnlineUsers     }
};


static std::map<RequestType, std::string> RequestTypeToString
{
        { RequestType::RequestHeartbeatPing     ,  "RequestHeartbeatPing"   }
    ,   { RequestType::RequestLogin             ,  "RequestLogin"           }
    ,   { RequestType::RequestLogout            ,  "RequestLogout"          }
    ,   { RequestType::RequestReceiveMessages   ,  "RequestReceiveMessages" }
    ,   { RequestType::RequestSendMessage       ,  "RequestSendMessage"     }
    ,   { RequestType::RequestOnlineUsers       ,  "RequestOnlineUsers"     }
};


static std::map<std::string, ResponseType> ResponseTypeFromString
{
        { "ResponseError"           ,   ResponseType::ResponseError             }
    ,   { "ResponseHeartbeatPong"   ,   ResponseType::ResponseHeartbeatPong     }
    ,   { "ResponseLogin"           ,   ResponseType::ResponseLogin             }
    ,   { "ResponseMessages"        ,   ResponseType::ResponseMessages          }
    ,   { "ResponseSuccess"         ,   ResponseType::ResponseSuccess           }
    ,   { "ResponseUsersList"       ,   ResponseType::ResponseUsersList         }
};


static std::map<ResponseType, std::string> ResponseTypeToString
{
        { ResponseType::ResponseError           ,  "ResponseError"             }
    ,   { ResponseType::ResponseHeartbeatPong   ,  "ResponseHeartbeatPong"     }
    ,   { ResponseType::ResponseLogin           ,  "ResponseLogin"             }
    ,   { ResponseType::ResponseMessages        ,  "ResponseMessages"          }
    ,   { ResponseType::ResponseSuccess         ,  "ResponseSuccess"           }
    ,   { ResponseType::ResponseUsersList       ,  "ResponseUsersList"         }
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
                      , data[PasswordKey].toString().toStdString());

        case RequestType::RequestSendMessage:
            return SendMessageRequest::fromJSON(clientId, jsonData);

        case RequestType::RequestOnlineUsers:
            return std::make_shared<OnlineUsersRequest>(
                        clientId
                      , data[AuthIdKey].toString().toStdString());

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


UsersListResponse::UsersListResponse(std::vector<std::string> usersList)
    : Response(ResponseType::ResponseUsersList)
    , usersList_(usersList)
{

}


QJsonObject UsersListResponse::toJson() const
{
    QJsonObject data = Response::toJson();

    QJsonArray usersJson;

    std::for_each(usersList_.begin(), usersList_.end(), [&](const std::string& userId){
        usersJson << QString::fromStdString(userId);
    });

    data[UsersKey] = usersJson;

    return data;
}


void UsersListResponse::handle(ResponseHandler& handler)
{
    handler.OnUsersList(*this);
}


std::shared_ptr<Response> UsersListResponse::fromJSON(const std::string& jsonData)
{
    QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

    std::vector<std::string> usersList;
    QJsonArray usersArray = data[UsersKey].toArray();
    foreach(auto userId, usersArray) {
        usersList.push_back(userId.toString().toStdString());
    }
    return std::make_shared<UsersListResponse>(std::move(usersList));
}


LoginRequest::LoginRequest(const std::string& clientId
                           , const std::string& authId
                           , const std::string& password)
    : Request (RequestType::RequestLogin, clientId)
    , authId_(authId)
    , password_(password)
{

}


QJsonObject LoginRequest::toJson() const
{
    QJsonObject data = Request::toJson();

    data[AuthIdKey] = QString::fromStdString(authId_);
    data[PasswordKey] = QString::fromStdString(password_);

    return data;
}


void LoginRequest::handle(RequestHandler& handler)
{
    handler.OnLogin(*this);
}



MessageData::MessageData(const QString& senderId
                        , const QString& receiverId
                        , const QDateTime& dateTime
                        , const QString& messageData)
    : senderId_(senderId)
    , receiverId_(receiverId)
    , dateTime_(dateTime)
    , messageData_(messageData)
{

}


QJsonObject MessageData::toJson() const
{
    QJsonObject data;

    data[SenderIdKey] = senderId_;
    data[ReceiverIdKey] = receiverId_;
    data[DateTimeKey] = dateTime_.toMSecsSinceEpoch();
    data[MessageKey] = messageData_;

    return data;
}


std::string MessageData::toJsonString() const
{
    return serializeData(this);
}


MessageData MessageData::fromJSON(const std::string& jsonData)
{
    QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

    QString senderId = data[SenderIdKey].toString();
    QString receiverId = data[ReceiverIdKey].toString();
    QDateTime dtm = QDateTime::fromMSecsSinceEpoch(data[DateTimeKey].toInt());
    QString messageData = data[MessageKey].toString();

    return MessageData(senderId, receiverId, dtm, messageData);
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
