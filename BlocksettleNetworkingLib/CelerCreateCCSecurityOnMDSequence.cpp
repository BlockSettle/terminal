#include "CelerCreateCCSecurityOnMDSequence.h"

#include <spdlog/spdlog.h>

#include "DownstreamSecurityProto.pb.h"
#include "UpstreamSecurityProto.pb.h"
#include "NettyCommunication.pb.h"

CelerCreateCCSecurityOnMDSequence::CelerCreateCCSecurityOnMDSequence(const std::string& securityId
      , const std::string& exchangeId
      , const callback_function& callback
      , const std::shared_ptr<spdlog::logger>& logger)
   : CelerCommandSequence("CelerCreateCCSecurityOnMDSequence",
      {
         { false, nullptr, &CelerCreateCCSecurityOnMDSequence::sendRequest }
       , { true, &CelerCreateCCSecurityOnMDSequence::processResponse, nullptr }
      })
   , securityId_{securityId}
   , exchangeId_{exchangeId}
   , callback_{callback}
   , logger_{logger}
{
}

bool CelerCreateCCSecurityOnMDSequence::FinishSequence()
{
   assert(callback_);

   callback_(result_, securityId_);

   return true;
}

CelerMessage CelerCreateCCSecurityOnMDSequence::sendRequest()
{
   com::celertech::staticdata::api::security::CreateSecurityListingRequest request;

   request.set_assettype(com::celertech::staticdata::api::enums::assettype::AssetType::CRYPTO);
   request.set_exchangecode("XCEL");
   request.set_securitycode(securityId_);
   request.set_securityid(securityId_);
   request.set_enabled(true);

   auto productType = request.add_parameter();
   productType->set_key("productType");
   productType->set_value("PRIVATE_SHARE");

   auto currencyCode = request.add_parameter();
   currencyCode->set_key("currencyCode");
   currencyCode->set_value("XBT");

   auto description = request.add_parameter();
   description->set_key("description");
   description->set_value("Definition created via PB");

   auto tickSize = request.add_parameter();
   tickSize->set_key("tickSize");
   tickSize->set_value("1");

   auto tickValue = request.add_parameter();
   tickValue->set_key("tickValue");
   tickValue->set_value("0.00001");

   auto exchangeId = request.add_parameter();
   exchangeId->set_key("exchangeId");
   exchangeId->set_value(GetExchangeId());

   auto alias = request.add_alias();
   alias->set_securityidsource("EXCHANGE");
   alias->set_securityidalias(securityId_);
   alias->set_securitycodealias(securityId_);

   logger_->debug("[CelerCreateCCSecurityOnMDSequence::sendRequest] {}"
      , request.DebugString());

   CelerMessage message;
   message.messageType = CelerAPI::CreateSecurityListingRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

std::string CelerCreateCCSecurityOnMDSequence::GetExchangeId() const
{
   return exchangeId_;
}

bool CelerCreateCCSecurityOnMDSequence::processResponse(const CelerMessage &message)
{
   com::celertech::baseserver::communication::protobuf::SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateCCSecurityOnMDSequence::processResponse] failed to parse massage of type {}", message.messageType);
      return false;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::SecurityListingDownstreamEventType) {
      logger_->error("[CelerCreateCCSecurityOnMDSequence::processResponse] unexpected type {} for class {}. Message: {}"
                     , payloadType, response.payload().classname()
                     , response.DebugString());
      return false;
   }

   com::celertech::staticdata::api::security::SecurityListingDownstreamEvent responseEvent;
   if (!responseEvent.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateCCSecurityOnMDSequence::processResponse] failed to parse SecurityListingDownstreamEvent");
      return false;
   }

   logger_->debug("[CelerCreateCCSecurityOnMDSequence::processResponse] get confirmation:\n{}"
                  , responseEvent.DebugString());

   result_ = true;
   return true;
}
