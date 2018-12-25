#ifndef __CHAT_PROTOCOL_H__
#define __CHAT_PROTOCOL_H__

#include <memory>
#include <map>
#include <vector>


#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QString>
#include <QDateTime>


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
   ,   RequestMessages
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

      ~Request() override = default;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId, const std::string& jsonData);
      virtual std::string getData() const;
      QJsonObject toJson() const override;
      virtual std::string getClientId() const { return clientId_; }
      virtual void handle(RequestHandler& handler) = 0;

   protected:
      std::string    clientId_;
   };


   class Response : public Message<ResponseType>
   {
   public:

      Response(ResponseType responseType)
         : Message<ResponseType> (responseType)
      {
      }
      ~Response() override = default;
      virtual std::string getData() const;
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      virtual void handle(ResponseHandler& handler) = 0;
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
                , const std::string& jwt);
      QJsonObject toJson() const override;
      void handle(RequestHandler& handler) override;
      std::string getAuthId() const { return authId_; }
      std::string getJWT() const { return jwt_; }

   private:
      std::string authId_;
      std::string jwt_;
   };


   class MessageData
   {
   public:
      MessageData(const QString& senderId
               , const QString& receiverId
               , const QDateTime& dateTime
               , const QString& messageData);
      QString getSenderId() const { return senderId_; }
      QString getReceiverId() const { return receiverId_; }
      QDateTime getDateTime() const { return dateTime_; }
      QString getMessageData() const { return messageData_; }
      QJsonObject toJson() const;
      std::string toJsonString() const;
      static std::shared_ptr<MessageData> fromJSON(const std::string& jsonData);

   private:
      QString senderId_;
      QString receiverId_;
      QDateTime dateTime_;
      QString messageData_;
   };


   class SendMessageRequest : public Request
   {
   public:
      SendMessageRequest(const std::string& clientId
                     , const std::string& messageData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler& handler) override;
      const std::string& getMessageData() const { return messageData_; }

   private:
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


   class MessagesRequest : public Request
   {
   public:
      MessagesRequest(const std::string& clientId
                  , const std::string& senderId
                  , const std::string& receiverId);
      QJsonObject toJson() const override;
      void handle(RequestHandler& handler) override;
      std::string getSenderId() const { return senderId_; }
      std::string getReceiverId() const { return receiverId_; }

   private:
      std::string senderId_;
      std::string receiverId_;
   };


   class HeartbeatPongResponse : public Response
   {
   public:
      HeartbeatPongResponse();
      void handle(ResponseHandler& handler) override;
   };


   class ListResponse : public Response
   {
   public:
      ListResponse(ResponseType responseType, std::vector<std::string> dataList);
      std::vector<std::string> getDataList() const { return dataList_; }
      static std::vector<std::string> fromJSON(const std::string& jsonData);
      QJsonObject toJson() const override;

   private:
      std::vector<std::string> dataList_;
   };


   class UsersListResponse : public ListResponse
   {
   public:
      UsersListResponse(std::vector<std::string> dataList);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler& handler) override;
   };


   class MessagesResponse : public ListResponse
   {
   public:
      MessagesResponse(std::vector<std::string> dataList);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler& handler) override;
   };


   class RequestHandler
   {
   public:
      virtual ~RequestHandler() = default;
      virtual void OnHeartbeatPing(HeartbeatPingRequest& request) = 0;
      virtual void OnLogin(LoginRequest& request) = 0;
      virtual void OnSendMessage(SendMessageRequest& request) = 0;
      virtual void OnOnlineUsers(OnlineUsersRequest& request) = 0;
      virtual void OnRequestMessages(MessagesRequest& request) = 0;
   };


   class ResponseHandler
   {
   public:
      virtual ~ResponseHandler() = default;
      virtual void OnHeartbeatPong(HeartbeatPongResponse& response) = 0;
      virtual void OnUsersList(UsersListResponse& response) = 0;
      virtual void OnMessages(MessagesResponse& response) = 0;
   };

}


#endif // __CHAT_PROTOCOL_H__
