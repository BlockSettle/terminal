#include "SingleCelerConnectionListener.h"

#include "ConnectionManager.h"

#include <spdlog/spdlog.h>

#include "NettyCommunication.pb.h"

using namespace com::celertech::baseserver::communication::protobuf;

SingleCelerConnectionListener::SingleCelerConnectionListener(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::string& name)
 : SingleConnectionServerListener(connectionManager->CreateCelerAPIServerConnection()
      , connectionManager->GetLogger(), name)
{}

bool SingleCelerConnectionListener::ProcessDataFromClient(const std::string& data)
{
   ProtobufMessage header;

   if (header.ParseFromString(data+'\0')) {
      logger_->error("[SingleCelerConnectionListener] {}: failed to parse ProtobufMessage({} bytes)"
         , GetName(), data.length());
      return false;
   }

   auto messageType = CelerAPI::GetMessageType(header.protobufclassname());
   if (messageType == CelerAPI::UndefinedType) {
      logger_->error("[SingleCelerConnectionListener] {}: get message of unrecognized type : {}"
         , GetName(), header.protobufclassname());
      return false;
   }

   int64_t sequenceNumber = -1;

   if (header.has_sequencenumber()) {
      sequenceNumber = header.sequencenumber();
   }

   if (messageType == CelerAPI::HeartbeatType) {
      return ProcessHeartBeat(sequenceNumber);
   }

   bool result = false;

   result = ProcessRequestDataFromClient(messageType, sequenceNumber, header.protobufmessagecontents());

   if (!result) {
      logger_->error("[SingleCelerConnectionListener::ProcessDataFromClient] {}: failed to process {}"
         , GetName(), header.protobufclassname());
   }

   return result;
}

bool SingleCelerConnectionListener::ProcessHeartBeat(int64_t sequenceNumber)
{
   return ReturnHeartbeat(sequenceNumber);
}

bool SingleCelerConnectionListener::ReturnHeartbeat(int64_t sequenceNumber)
{
   Heartbeat response;

   std::string className = CelerAPI::GetMessageClass(CelerAPI::HeartbeatType);
   if (className.empty()) {
      logger_->error("[SingleCelerConnectionListener::ReturnHeartbeat] could not get class name for HeartbeatType");
      return false;
   }

   ProtobufMessage message;

   message.set_protobufclassname(className);
   if (sequenceNumber != -1) {
      message.set_sequencenumber(sequenceNumber);
   }
   message.set_protobufmessagecontents(response.SerializeAsString());

   if (!SendDataToClient(message.SerializeAsString())) {
      logger_->error("[SingleCelerConnectionListener::ReturnHeartbeat] {} : failed to send response to client"
         , GetName());
      return false;
   }

   return true;
}
