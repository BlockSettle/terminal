#ifndef CHATPROTOCOL_H
#define CHATPROTOCOL_H


#include <memory>
#include <map>
#include <vector>


#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QString>


namespace Chat
{
    class RequestHandler;
    class ResponseHandler;


    enum class RequestType
    {
        RequestHeartbeatPing
    ,   RequestLogin
    ,   RequestLogout
    ,   RequestSendMessage
    ,   RequestReceiveMessages
    ,   RequestOnlineUsers

    };


    enum class ResponseType
    {
        ResponseHeartbeatPong
    ,   ResponseLogin
    ,   ResponseMessages
    ,   ResponseSuccess
    ,   ResponseError
    ,   ResponseUsersList

    };


    template <typename T>
    class Message
    {
    public:

        Message(T messageType)
            : messageType_(messageType)
            , version_("1.0.0")
        {

        }

        virtual ~Message() = default;

        std::string getVersion() const { return version_; }

        virtual QJsonObject toJson() const;


    protected:

        static std::string serializeData(const Message<T>* thisPtr)
        {
            auto data = QJsonDocument(thisPtr->toJson());
            QString serializedData = QString::fromUtf8(data.toJson());
            return serializedData.toStdString();
        }


    protected:

        T messageType_;

        std::string version_;

    };


    class Request : public Message<RequestType>
    {
    public:

        Request(RequestType requestType, const std::string& clientId)
            : Message<RequestType> (requestType)
            , clientId_(clientId)
        {

        }

        virtual ~Request() override = default;

        static std::shared_ptr<Request> fromJSON(const std::string& clientId, const std::string& jsonData);

        virtual std::string getData() const;

        QJsonObject toJson() const override;

        virtual std::string getClientId() const { return clientId_; }

        virtual void handle(RequestHandler& handler) = 0;


    protected:

        std::string     clientId_;

    };


    class Response : public Message<ResponseType>
    {
    public:

        Response(ResponseType responseType)
            : Message<ResponseType> (responseType)
        {

        }

        virtual ~Response() override = default;

        virtual std::string getData() const;

        QJsonObject toJson() const override;

        static std::shared_ptr<Response> fromJSON(const std::string& jsonData);

        virtual void handle(ResponseHandler& handler) = 0;


    protected:

    };


    class HeartbeatPingRequest : public Request
    {
    public:

        HeartbeatPingRequest(const std::string& clientId);

        void handle(RequestHandler& handler) override;
    };


    class LoginRequest : public Request
    {
    public:

        LoginRequest(const std::string& clientId
                     , const std::string& authId
                     , const std::string& password);
        QJsonObject toJson() const override;

        void handle(RequestHandler& handler) override;

        std::string getAuthId() const { return authId_; }
        std::string getPassword() const { return password_; }

    private:
        std::string authId_;
        std::string password_;
    };


    class SendMessageRequest : public Request
    {
    public:

        SendMessageRequest(const std::string& clientId
                           , const std::string& senderId
                           , const std::string& receiverId
                           , const std::string& messageData);

        QJsonObject toJson() const override;

        void handle(RequestHandler& handler) override;

        std::string getSenderId() const { return senderId_; }
        std::string getReceiverId() const { return receiverId_; }
        std::string getMessageData() const { return messageData_; }

    private:

        std::string senderId_;
        std::string receiverId_;
        std::string messageData_;

    };


    class OnlineUsersRequest : public Request
    {
    public:

        OnlineUsersRequest(const std::string& clientId
                           , const std::string& authId);

        QJsonObject toJson() const override;

        void handle(RequestHandler& handler) override;

        std::string getAuthId() const { return authId_; }

    private:

        std::string authId_;
    };


    class HeartbeatPongResponse : public Response
    {
    public:

        HeartbeatPongResponse();

        void handle(ResponseHandler& handler) override;
    };


    class UsersListResponse : public Response
    {
    public:

        UsersListResponse(std::vector<std::string> usersList);

        void handle(ResponseHandler& handler) override;

        std::vector<std::string> getUsersList() const { return usersList_; }

        QJsonObject toJson() const override;

        static std::shared_ptr<Response> fromJSON(const std::string& jsonData);


    private:

        std::vector<std::string> usersList_;
    };


    class RequestHandler
    {

    public:

        virtual ~RequestHandler() = default;

        virtual void OnHeartbeatPing(HeartbeatPingRequest& request) = 0;

        virtual void OnLogin(LoginRequest& request) = 0;

        virtual void OnSendMessage(SendMessageRequest& request) = 0;

        virtual void OnOnlineUsers(OnlineUsersRequest& request) = 0;
    };


    class ResponseHandler
    {

    public:

        virtual ~ResponseHandler() = default;

        virtual void OnHeartbeatPong(HeartbeatPongResponse& response) = 0;

        virtual void OnUsersList(UsersListResponse& response) = 0;
    };

}


#endif // CHATPROTOCOL_H
