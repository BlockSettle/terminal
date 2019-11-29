/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CCPubConnection.h"

#include "ConnectionManager.h"
#include "RequestReplyCommand.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include "bs_communication.pb.h"

#include <spdlog/spdlog.h>

using namespace Blocksettle::Communication;

CCPubConnection::CCPubConnection(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const ZmqBipNewKeyCb &cb)
   : logger_{logger}
   , connectionManager_{connectionManager}
   , cbApproveConn_(cb)
{}

bool CCPubConnection::LoadCCDefinitionsFromPub()
{
   GetCCGenesisAddressesRequest genAddrReq;
   RequestPacket  request;

   genAddrReq.set_networktype(IsTestNet()
      ? AddressNetworkType::TestNetType
      : AddressNetworkType::MainNetType);

   if (currentRev_ > 0) {
      genAddrReq.set_hasrevision(currentRev_);
   }

   request.set_requesttype(GetCCGenesisAddressesType);
   request.set_requestdata(genAddrReq.SerializeAsString());

   return SubmitRequestToPB("get_cc_gen_list", request.SerializeAsString());
}

bool CCPubConnection::SubmitRequestToPB(const std::string &name, const std::string& data)
{
   const auto connection = connectionManager_->CreateZMQBIP15XDataConnection();
   connection->setCBs(cbApproveConn_);

   cmdPuB_ = std::make_shared<RequestReplyCommand>(name, connection, logger_);

   cmdPuB_->SetReplyCallback([this] (const std::string& data) {
      OnDataReceived(data);

      QMetaObject::invokeMethod(this, [this] {
         cmdPuB_->CleanupCallbacks();
         cmdPuB_->resetConnection();
      });
      return true;
   });

   cmdPuB_->SetErrorCallback([this](const std::string& message) {
      logger_->error("[CCPubConnection::SubmitRequestToPB] error callback {}: {}"
         , cmdPuB_->GetName(), message);

      QMetaObject::invokeMethod(this, [this] {
         cmdPuB_->CleanupCallbacks();
         cmdPuB_->resetConnection();
      });
   });

   if (!cmdPuB_->ExecuteRequest(GetPuBHost(), GetPuBPort(), data, true)) {
      logger_->error("[CCPubConnection::{}] failed to send request {}", __func__, name);
      return false;
   }

   return true;
}

void CCPubConnection::OnDataReceived(const std::string& data)
{
   if (data.empty()) {
      return;
   }
   ResponsePacket response;

   if (!response.ParseFromString(data)) {
      logger_->error("[CCPubConnection::OnDataReceived] failed to parse response from public bridge");
      return;
   }

   switch (response.responsetype()) {
   case RequestType::GetCCGenesisAddressesType:
      ProcessGenAddressesResponse(response.responsedata(), response.datasignature());
      break;
   case RequestType::SubmitCCAddrInitialDistribType:
      ProcessSubmitAddrResponse(response.responsedata());
      break;
   case RequestType::ErrorMessageResponseType:
      ProcessErrorResponse(response.responsedata());
      break;
   default:
      logger_->error("[CCPubConnection::OnDataReceived] unrecognized response type from public bridge: {}", response.responsetype());
      break;
   }
}

void CCPubConnection::ProcessErrorResponse(const std::string& responseString) const
{
   ErrorMessageResponse response;
   if (!response.ParseFromString(responseString)) {
      logger_->error("[CCPubConnection::ProcessErrorResponse] failed to parse error message response");
      return;
   }

   logger_->error("[CCPubConnection::ProcessErrorResponse] error message from public bridge: {}", response.errormessage());
}
