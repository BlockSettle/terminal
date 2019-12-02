/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerCreateCCSecurityOnMDSequence.h"

#include <spdlog/spdlog.h>

#include "UpstreamSecurityProto.pb.h"
#include "NettyCommunication.pb.h"

CelerCreateCCSecurityOnMDSequence::CelerCreateCCSecurityOnMDSequence(const std::string& securityId
      , const std::string& exchangeId
      , const std::shared_ptr<spdlog::logger>& logger)
   : CelerCommandSequence("CelerCreateCCSecurityOnMDSequence",
      {
         { false, nullptr, &CelerCreateCCSecurityOnMDSequence::sendRequest }
      })
   , securityId_{securityId}
   , exchangeId_{exchangeId}
   , logger_{logger}
{
}

bool CelerCreateCCSecurityOnMDSequence::FinishSequence()
{
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
   description->set_value("Definition created via MD");

   auto tickSize = request.add_parameter();
   tickSize->set_key("tickSize");
   tickSize->set_value("1");

   auto divisor = request.add_parameter();
   divisor->set_key("divisor");
   divisor->set_value("1");

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

   CelerMessage message;
   message.messageType = CelerAPI::CreateSecurityListingRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

std::string CelerCreateCCSecurityOnMDSequence::GetExchangeId() const
{
   return exchangeId_;
}
