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
static const QString ToIdKey = QStringLiteral("toid");
static const QString FromIdKey = QStringLiteral("fromid");
static const QString StatusKey = QStringLiteral("status");


static std::map<std::string, RequestType> RequestTypeFromString
{
        { "RequestHeartbeatPing"    ,   RequestType::RequestHeartbeatPing   }
    ,   { "RequestLogin"            ,   RequestType::RequestLogin           }
    ,   { "RequestLogout"           ,   RequestType::RequestLogout          }
    ,   { "RequestReceiveMessages"  ,   RequestType::RequestReceiveMessages }
    ,   { "RequestSendMessage"      ,   RequestType::RequestSendMessage     }
};


static std::map<RequestType, std::string> RequestTypeToString
{
        { RequestType::RequestHeartbeatPing     ,  "RequestHeartbeatPing"   }
    ,   { RequestType::RequestLogin             ,  "RequestLogin"           }
    ,   { RequestType::RequestLogout            ,  "RequestLogout"          }
    ,   { RequestType::RequestReceiveMessages   ,  "RequestReceiveMessages" }
    ,   { RequestType::RequestSendMessage       ,  "RequestSendMessage"     }
};


static std::map<std::string, ResponseType> ResponseTypeFromString
{
        { "ResponseError"           ,   ResponseType::ResponseError             }
    ,   { "ResponseHeartbeatPong"   ,   ResponseType::ResponseHeartbeatPong     }
    ,   { "ResponseLogin"           ,   ResponseType::ResponseLogin             }
    ,   { "ResponseMessages"        ,   ResponseType::ResponseMessages          }
    ,   { "ResponseSuccess"         ,   ResponseType::ResponseSuccess           }
};


static std::map<ResponseType, std::string> ResponseTypeToString
{
        { ResponseType::ResponseError           ,  "ResponseError"             }
    ,   { ResponseType::ResponseHeartbeatPong   ,  "ResponseHeartbeatPong"     }
    ,   { ResponseType::ResponseLogin           ,  "ResponseLogin"             }
    ,   { ResponseType::ResponseMessages        ,  "ResponseMessages"          }
    ,   { ResponseType::ResponseSuccess         ,  "ResponseSuccess"           }
};


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
        default:
            break;
    }

    return std::shared_ptr<Request>();
}


std::string Request::getData() const
{
    return Message<RequestType>::serializeData(this);
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
    return Message<ResponseType>::serializeData(this);
}


std::shared_ptr<Response> Response::fromJSON(const std::string& jsonData)
{
    QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

    ResponseType responseType = ResponseTypeFromString[data[TypeKey].toString().toStdString()];

    switch (responseType)
    {
        case ResponseType::ResponseHeartbeatPong:
            return std::make_shared<HeartbeatPongResponse>();

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
