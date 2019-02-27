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

#include "SecureBinaryData.h"
#include "EncryptUtils.h"


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
   ,   RequestAskForPublicKey
   ,   RequestSendOwnPublicKey
   ,   RequestChangeMessageStatus
   };


   enum class ResponseType
   {
       ResponseHeartbeatPong
   ,   ResponseLogin
   ,   ResponseMessages
   ,   ResponseSuccess
   ,   ResponseError
   ,   ResponseUsersList
   ,   ResponseAskForPublicKey
   ,   ResponseSendOwnPublicKey
   ,   ResponsePendingMessage
   ,   ResponseSendMessage
   ,   ResponseChangeMessageStatus
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
      virtual void handle(RequestHandler &) = 0;

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
      virtual void handle(ResponseHandler &) = 0;
     ResponseType getType() { return messageType_; }
   };

   class HeartbeatPingRequest : public Request
   {
   public:
      HeartbeatPingRequest(const std::string& clientId);
      void handle(RequestHandler &) override;
   };

   class BaseLoginRequest : public Request
   {
   public:
      BaseLoginRequest(RequestType requestType
                , const std::string& clientId
                , const std::string& authId
                , const std::string& jwt);
      QJsonObject toJson() const override;
      std::string getAuthId() const { return authId_; }
      std::string getJWT() const { return jwt_; }

   protected:
      std::string authId_;
      std::string jwt_;
   };


   class LoginRequest : public BaseLoginRequest
   {
   public:
      LoginRequest(const std::string& clientId
                  , const std::string& authId
                  , const std::string& jwt)
         : BaseLoginRequest (RequestType::RequestLogin, clientId, authId, jwt)
      {
      }
      void handle(RequestHandler &) override;
   };

   class LogoutRequest : public BaseLoginRequest
   {
   public:
      LogoutRequest(const std::string& clientId
                  , const std::string& authId
                  , const std::string& jwt)
         : BaseLoginRequest (RequestType::RequestLogout, clientId, authId, jwt)
      {
      }
      void handle(RequestHandler &) override;
   };


   class MessageData
   {
   public:
      enum class State {
         Undefined = 0,
         Invalid = 1,
         Encrypted = 2,
         Acknowledged = 4,
         Read = 8,
         Sent = 16
      };

      MessageData(const QString &sender, const QString &receiver
         , const QString &id, const QDateTime &dateTime
         , const QString& messageData, int state = (int)State::Undefined);
      QString getSenderId() const { return senderId_; }
      QString getReceiverId() const { return receiverId_; }
      QString getId() const { return id_; }
      QDateTime getDateTime() const { return dateTime_; }
      QString getMessageData() const { return messageData_; }
      int getState() const { return state_; }
      QJsonObject toJson() const;
      std::string toJsonString() const;
      static std::shared_ptr<MessageData> fromJSON(const std::string& jsonData);

      void setFlag(const State);
      void unsetFlag(const State);
      bool decrypt(const autheid::PrivateKey& privKey);
      bool encrypt(const autheid::PublicKey& pubKey);
      
      //Set ID for message, returns old ID that was replaced
      QString setId(const QString& id);

   private:
      QString id_;
      QString senderId_;
      QString receiverId_;
      QDateTime dateTime_;
      QString messageData_;
      int state_;
   };


   class SendMessageRequest : public Request
   {
   public:
      SendMessageRequest(const std::string& clientId
                     , const std::string& messageData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler &) override;
      const std::string& getMessageData() const { return messageData_; }

   private:
      std::string messageData_;
   };
   
   class MessageChangeStatusRequest : public Request
   {
   public:
      MessageChangeStatusRequest(const std::string& clientId, const std::string& messageId, int state);
      
      const std::string getMessageId() const {return messageId_; }
      int getMessageState() const {return messageState_; }
      
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler &) override;
   private:
      const std::string messageId_;
      int messageState_;
   };

   // Request for asking the peer to send us their public key.
   class AskForPublicKeyRequest : public Request
   {
   public:
      AskForPublicKeyRequest(
         const std::string& clientId,
         const std::string& askingNodeId,
         const std::string& peerId);

      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(
         const std::string& clientId,
         const std::string& jsonData);

      void handle(RequestHandler &) override;

      const std::string& getAskingNodeId() const;
      const std::string& getPeerId() const;

   private:
      std::string askingNodeId_;
      std::string peerId_;
   };

   // Request for sending our key to the peer, who previously asked for it.
   class SendOwnPublicKeyRequest : public Request
   {
   public:
      SendOwnPublicKeyRequest(
         const std::string& clientId,
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const autheid::PublicKey& sendingNodePublicKey);

      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(
         const std::string& clientId,
         const std::string& jsonData);

      void handle(RequestHandler &) override;

      const std::string& getReceivingNodeId() const;
      const std::string& getSendingNodeId() const;
      const autheid::PublicKey& getSendingNodePublicKey() const;
      
   private:
      std::string receivingNodeId_;
      std::string sendingNodeId_;
      autheid::PublicKey sendingNodePublicKey_;
   };

   class OnlineUsersRequest : public Request
   {
   public:
      OnlineUsersRequest(const std::string& clientId
                     , const std::string& authId);
      QJsonObject toJson() const override;
      void handle(RequestHandler &) override;
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
      void handle(RequestHandler &) override;
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
      void handle(ResponseHandler &) override;
   };

   class LoginResponse : public Response
   {
   public:
      enum class Status {
           LoginOk
         , LoginFailed
      };

      LoginResponse(const std::string& userId, Status status);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      QJsonObject toJson() const override;

      std::string getUserId() const { return userId_; }
      Status getStatus() const { return status_; }

   private:
      std::string userId_;
      Status status_;
   };

   // Response to ask a peer to send us his own public key.
   // Strangely, this response is sent to the peer itself, not the one who sent the request.
   class AskForPublicKeyResponse : public Response
   {
   public:
      AskForPublicKeyResponse(
         const std::string& askingNodeId,
         const std::string& peerId);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(
         const std::string& jsonData);

      void handle(ResponseHandler &) override;

      const std::string& getAskingNodeId() const;
      const std::string& getPeerId() const;

   private:
      std::string askingNodeId_;
      std::string peerId_;
   };
 
   // Response to sending our own public key to the peer who asked for it.
   // Strangely, Response is sent to the peer to whom the key is being sent, not the 
   // node which made the call.
   class SendOwnPublicKeyResponse : public Response
   {
   public:
      SendOwnPublicKeyResponse(
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const autheid::PublicKey& sendingNodePublicKey);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(
         const std::string& jsonData);

      void handle(ResponseHandler &) override;

      const std::string& getReceivingNodeId() const;
      const std::string& getSendingNodeId() const;
      const autheid::PublicKey& getSendingNodePublicKey() const;

   private:
      std::string receivingNodeId_;
      std::string sendingNodeId_;
      autheid::PublicKey sendingNodePublicKey_;
   };
  
   class ListResponse : public Response
   {
   public:
      ListResponse(ResponseType responseType, std::vector<std::string> dataList);
      std::vector<std::string> getDataList() const { return dataList_; }
      static std::vector<std::string> fromJSON(const std::string& jsonData);
      QJsonObject toJson() const override;

   protected:
      std::vector<std::string> dataList_;
   };

   class UsersListResponse : public ListResponse
   {
   public:
      enum class Command {
         Replace = 0,
         Add,
         Delete
      };
      UsersListResponse(std::vector<std::string> dataList, Command cmd = Command::Replace);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      QJsonObject toJson() const override;
      void handle(ResponseHandler &) override;
      Command command() const { return cmd_; }

   private:
      Command  cmd_;
   };

   class MessagesResponse : public ListResponse
   {
   public:
      MessagesResponse(std::vector<std::string> dataList);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
   };
   
   class PendingResponse : public Response
   {
   public:
      PendingResponse(ResponseType type, const QString &id = QString());
      QJsonObject toJson() const override;
      QString getId() const;
      void setId(const QString& id);
      void handle(ResponseHandler &) override;
   private:
      QString id_;
      
   };

   class PendingMessagesResponse : public PendingResponse
   {
   public: 
      PendingMessagesResponse(const QString& message_id, const QString& id = QString());
      QString getMessageId();
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
   protected:
      QString message_id_;
   };
   
   class SendMessageResponse : public PendingResponse
   {
   public:
      
      enum class Result {
           Accepted
         , Rejected
      };
      SendMessageResponse(const std::string& clientMessageId, const std::string& serverMessageId, const std::string& receiverId, Result result);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      
      std::string clientMessageId() const { return clientMessageId_;}
      std::string serverMessageId() const { return serverMessageId_;}
      std::string receiverId() const { return receiverId_;}
      Result getResult() const {return result_;}
      void handle(ResponseHandler&) override;
      
   private:
      std::string clientMessageId_;
      std::string serverMessageId_;
      std::string receiverId_;
      Result result_;
   };
   
   class MessageChangeStatusResponse : public PendingResponse
   {
   public:
      MessageChangeStatusResponse(const std::string& messageId, const std::string& senderId,const std::string& receiverId, int status);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      std::string messageId() const {return messageId_;} 
      std::string messageSenderId() const {return messageSenderId_;}
      std::string messageReceiverId() const {return messageReceiverId_;}
      int getUpdatedStatus() const {return status_; }
      void handle(ResponseHandler&) override;
   private:
      std::string messageId_;
      std::string messageSenderId_;
      std::string messageReceiverId_;
      int status_;
   };


   class RequestHandler
   {
   public:
      virtual ~RequestHandler() = default;
      virtual void OnHeartbeatPing(const HeartbeatPingRequest& request) = 0;
      virtual void OnLogin(const LoginRequest &) = 0;
      virtual void OnLogout(const LogoutRequest &) = 0;
      virtual void OnSendMessage(const SendMessageRequest &) = 0;

      // Asking peer to send us their public key.
      virtual void OnAskForPublicKey(const AskForPublicKeyRequest &) = 0;

      // Sending our public key to the peer who asked for it.
      virtual void OnSendOwnPublicKey(const SendOwnPublicKeyRequest &) = 0;

      virtual void OnOnlineUsers(const OnlineUsersRequest &) = 0;
      virtual void OnRequestMessages(const MessagesRequest &) = 0;
      
      virtual void OnRequestChangeMessageStatus(const MessageChangeStatusRequest &) = 0;
   };

   class ResponseHandler
   {
   public:
      virtual ~ResponseHandler() = default;
      virtual void OnHeartbeatPong(const HeartbeatPongResponse &) = 0;
      virtual void OnUsersList(const UsersListResponse &) = 0;
      virtual void OnMessages(const MessagesResponse &) = 0;

      // Received a call from a peer to send our public key.
      virtual void OnAskForPublicKey(const AskForPublicKeyResponse &) = 0;
      
      // Received public key of one of our peers.
      virtual void OnSendOwnPublicKey(const SendOwnPublicKeyResponse &) = 0;

      virtual void OnLoginReturned(const LoginResponse &) = 0;
      
      virtual void OnSendMessageResponse(const SendMessageResponse&) = 0;
      virtual void OnMessageChangeStatusResponse(const MessageChangeStatusResponse&) = 0;
   };

}


#endif // __CHAT_PROTOCOL_H__
