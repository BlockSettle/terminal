/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerLoadMDDefinitionsSequence.h"

#include <spdlog/spdlog.h>

#include "NettyCommunication.pb.h"
#include "UpstreamSecurityProto.pb.h"

CelerLoadMDDefinitionsSequence::CelerLoadMDDefinitionsSequence(const std::shared_ptr<spdlog::logger>& logger)
   : CelerCommandSequence("CelerLoadMDDefinitionsSequence",
      {
         { false, nullptr, &CelerLoadMDDefinitionsSequence::sendRequest }
       , { true, &CelerLoadMDDefinitionsSequence::processResponse, nullptr }
      })
   , logger_{logger}
{
}

bool CelerLoadMDDefinitionsSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerLoadMDDefinitionsSequence::sendRequest()
{
   com::celertech::staticdata::api::security::FindAllSecurityListingsRequest request;

   CelerMessage message;
   message.messageType = CelerAPI::FindAllSecurityListingsRequestType;
   message.messageData = request.SerializeAsString();

   logger_->debug("[CelerLoadMDDefinitionsSequence::sendRequest] requesting all security definitions");

   return message;
}

bool CelerLoadMDDefinitionsSequence::processResponse(const CelerMessage &message)
{
   com::celertech::baseserver::communication::protobuf::MultiResponseMessage response;

   if (message.messageType != CelerAPI::MultiResponseMessageType) {
      logger_->error("[CelerLoadMDDefinitionsSequence::processResponse] unexpected message type: {}"
                     , CelerAPI::GetMessageClass(message.messageType));
      return false;
   }

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerLoadMDDefinitionsSequence::processResponse] failed to parse MultiResponseMessage");
      return false;
   }

   logger_->debug("[CelerLoadMDDefinitionsSequence::processResponse] get {} payloads"
      , response.payload_size());

   for (int i = 0; i < response.payload_size(); i++) {
      const auto& payload = response.payload(i);

      logger_->debug("[CelerLoadMDDefinitionsSequence::processResponse] get payload of type", payload.classname());
   }

   return true;
}
