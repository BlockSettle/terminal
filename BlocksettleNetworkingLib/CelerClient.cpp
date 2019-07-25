#include "CelerClient.h"

#include <spdlog/spdlog.h>
#include "ConnectionManager.h"
#include "DataConnection.h"
#include "NettyCommunication.pb.h"

class CelerClientListener : public DataConnectionListener
{
public:
   void OnDataReceived(const std::string& data) override
   {
      com::celertech::baseserver::communication::protobuf::ProtobufMessage celerMsg;

      bool result = celerMsg.ParseFromString(data);
      if (!result) {
         SPDLOG_LOGGER_CRITICAL(logger_, "failed to parse ProtobufMessage from Celer");
         return;
      }

      auto messageType = CelerAPI::GetMessageType(celerMsg.protobufclassname());
      if (!CelerAPI::isValidMessageType(messageType)) {
         SPDLOG_LOGGER_CRITICAL(logger_, "get message of unrecognized type: {}", celerMsg.protobufclassname());
         return;
      }

      client_->recvData(messageType, celerMsg.protobufmessagecontents());
   }
   void OnConnected() override
   {
      client_->OnConnected();
   }
   void OnDisconnected() override
   {
      client_->OnDisconnected();
   }
   void OnError(DataConnectionListener::DataConnectionError errorCode) override
   {
      client_->OnError(errorCode);
   }

   BaseCelerClient *client_{};
   std::shared_ptr<spdlog::logger> logger_;
};

CelerClient::CelerClient(const std::shared_ptr<ConnectionManager> &connectionManager, bool userIdRequired)
   : BaseCelerClient(connectionManager->GetLogger(), userIdRequired)
   , connectionManager_(connectionManager)
{
}

CelerClient::~CelerClient() = default;

bool CelerClient::LoginToServer(const std::string &hostname, const std::string &port
   , const std::string &login, const std::string &password)
{
   if (connection_) {
      logger_->error("[CelerClient::LoginToServer] connecting with not purged connection");
      return false;
   }

   listener_ = std::make_unique<CelerClientListener>();
   listener_->client_ = this;
   listener_->logger_ = logger_;

   connection_ = connectionManager_->CreateCelerClientConnection();

   // put login command to queue
   if (!connection_->openConnection(hostname, port, listener_.get())) {
      logger_->error("[CelerClient::LoginToServer] failed to open celer connection");
      // XXX purge connection and listener
      connection_.reset();
      return false;
   }

   bool result = BaseCelerClient::SendLogin(login, login, password);
   return result;
}

void CelerClient::CloseConnection()
{
   if (connection_) {
      connection_->closeConnection();
      connection_.reset();
   }

   BaseCelerClient::CloseConnection();
}

void CelerClient::onSendData(CelerAPI::CelerMessageType messageType, const std::string &data)
{
   std::string fullClassName = CelerAPI::GetMessageClass(messageType);
   assert(!fullClassName.empty());

   com::celertech::baseserver::communication::protobuf::ProtobufMessage message;
   message.set_protobufclassname(fullClassName);
   message.set_protobufmessagecontents(data);
   connection_->send(message.SerializeAsString());
}
