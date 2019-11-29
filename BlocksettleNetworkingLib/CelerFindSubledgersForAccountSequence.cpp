/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerFindSubledgersForAccountSequence.h"

#include <spdlog/logger.h>

#include "UpstreamSubLedgerProto.pb.h"
#include "DownstreamSubLedgerProto.pb.h"
using namespace com::celertech::piggybank::api::subledger;

#include "NettyCommunication.pb.h"
using namespace com::celertech::baseserver::communication::protobuf;

CelerFindSubledgersForAccountSequence::CelerFindSubledgersForAccountSequence(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& accountName
      , const onAccountBalanceLoaded& cb)
: CelerCommandSequence("CelerFindSubledgersForAccountSequence",
      {
         { false, nullptr, &CelerFindSubledgersForAccountSequence::sendFindSubledgersRequest}
       , { true, &CelerFindSubledgersForAccountSequence::processFindSubledgersResponse, nullptr}
      })
 , logger_(logger)
 , cb_(cb)
 , accountName_(accountName)
{}

bool CelerFindSubledgersForAccountSequence::FinishSequence()
{
   if (cb_) {
      cb_(balancePairs_);
   }
   return true;
}

CelerMessage CelerFindSubledgersForAccountSequence::sendFindSubledgersRequest()
{

   logger_->debug("[CelerFindSubledgersForAccountSequence::sendFindSubledgersRequest] send request for {}"
      , accountName_);

   FindAllSubLedgersByAccount request;
   request.set_clientrequestid(GetSequenceId());
   request.set_account(accountName_);

   CelerMessage message;
   message.messageType = CelerAPI::FindAllSubLedgersByAccountType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerFindSubledgersForAccountSequence::processFindSubledgersResponse(const CelerMessage& message)
{
   if (message.messageType != CelerAPI::MultiResponseMessageType) {
      logger_->error("[CelerFindSubledgersForAccountSequence::processFindSubledgersResponse] invalid response type {}", message.messageType);
      return false;
   }

   MultiResponseMessage response;
   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerFindSubledgersForAccountSequence::processFindSubledgersResponse] failed to parse MultiResponseMessage");
      return false;
   }

   logger_->debug("[CelerFindSubledgersForAccountSequence::processFindSubledgersResponse] get response: {}", response.payload_size());

   for (int i=0; i < response.payload_size(); ++i) {
      const ResponsePayload& payload = response.payload(i);
      auto payloadType = CelerAPI::GetMessageType(payload.classname());
      if (payloadType != CelerAPI::SubLedgerSnapshotDownstreamEventType) {
         logger_->error("[CelerFindSubledgersForAccountSequence::processFindSubledgersResponse] invalid payload type {}", payload.classname());
         return false;
      }

      SubLedgerSnapshotDownstreamEvent subledger;
      if (!subledger.ParseFromString(payload.contents())) {
         logger_->error("[CelerFindSubledgersForAccountSequence::processFindSubledgersResponse] failed to parse SubLedgerSnapshotDownstreamEvent");
         return false;
      }

      if (subledger.currency() != "USD") {
         balancePairs_.emplace_back(subledger.currency(), subledger.netposition());
      }
   }

   return true;
}
