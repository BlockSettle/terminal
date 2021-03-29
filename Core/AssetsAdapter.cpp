/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AssetsAdapter.h"
#include <spdlog/spdlog.h>
#include "TerminalMessage.h"

#include "terminal.pb.h"

using namespace bs::message;
using namespace BlockSettle::Terminal;


AssetsAdapter::AssetsAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Assets))
{
   assetMgr_ = std::make_unique<AssetManager>(logger, this);
}

bool AssetsAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.id);
         return true;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         SettingsMessage msgSet;
         auto msgReq = msgSet.mutable_get_request();
         auto setReq = msgReq->add_requests();
         setReq->set_source(SettingSource_Local);
         setReq->set_index(SetIdx_BlockSettleSignAddress);
         Envelope envReq{ 0, user_, UserTerminal::create(TerminalUsers::Settings)
            , {}, {}, msgSet.SerializeAsString() };
         pushFill(envReq);
      }
   }
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kGetResponse:
         return processGetSettings(msg.get_response());
      case SettingsMessage::kBootstrap:
         return processBootstrap(msg.bootstrap());
      default: break;
      }
   }
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::Matching) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse matching message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kLoggedIn:
         return onMatchingLogin(msg.logged_in());
      case MatchingMessage::kSubmittedAuthAddresses:
         return processSubmittedAuth(msg.submitted_auth_addresses());
      default: break;
      }
   } 
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::BsServer) {
      BsServerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse BS message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case BsServerMessage::kBalanceUpdated:
         return processBalance(msg.balance_updated().currency(), msg.balance_updated().value());
      default: break;
      }
   }
   else if (env.receiver && (env.receiver->value() == user_->value())) {
      AssetsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case AssetsMessage::kSubmitAuthAddress:
         return processSubmitAuth(msg.submit_auth_address());
      default: break;
      }
   }
   return true;
}

void AssetsAdapter::onCcPriceChanged(const std::string& currency)
{
}

void AssetsAdapter::onXbtPriceChanged(const std::string& currency)
{
}

void AssetsAdapter::onFxBalanceLoaded()
{
}

void AssetsAdapter::onFxBalanceCleared()
{
}

void AssetsAdapter::onBalanceChanged(const std::string& currency)
{
}

void AssetsAdapter::onTotalChanged()
{
}

void AssetsAdapter::onSecuritiesChanged()
{
}

void AssetsAdapter::onCCSecurityDef(const bs::network::CCSecurityDef& sd)
{
   AssetsMessage msg;
   auto msgCC = msg.mutable_cc_definition();
   msgCC->set_security_id(sd.securityId);
   msgCC->set_product(sd.product);
   msgCC->set_genesis_address(sd.genesisAddr.display());
   msgCC->set_lot_size(sd.nbSatoshis);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void AssetsAdapter::onLoaded()
{
   logger_->debug("[{}]", __func__);
   AdministrativeMessage admMsg;
   admMsg.set_component_loading(user_->value());
   Envelope envBC{ 0, UserTerminal::create(TerminalUsers::System), nullptr
      , {}, {}, admMsg.SerializeAsString() };
   pushFill(envBC);

   AssetsMessage msg;
   msg.mutable_loaded();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

bool AssetsAdapter::processGetSettings(const SettingsMessage_SettingsResponse& response)
{
   for (const auto& setting : response.responses()) {
      switch (setting.request().index()) {
      case SetIdx_BlockSettleSignAddress:
         onBSSignAddress(setting.s());
         break;
      default: break;
      }
   }
   return true;
}

void AssetsAdapter::onBSSignAddress(const std::string& address)
{
   ccFileMgr_ = std::make_unique<CCFileManager>(logger_, this, address);

   SettingsMessage msgSet;
   auto msgReq = msgSet.mutable_get_bootstrap();
   Envelope envReq{ 0, user_, UserTerminal::create(TerminalUsers::Settings)
      , {}, {}, msgSet.SerializeAsString() };
   pushFill(envReq);
}

bool AssetsAdapter::processBootstrap(const SettingsMessage_BootstrapData& response)
{
   if (!ccFileMgr_) {
      logger_->debug("[{}] CC file manager is not ready, yet", __func__);
      return false;
   }
   logger_->debug("[{}]", __func__);
   std::vector<bs::network::CCSecurityDef> ccDefs;
   ccDefs.reserve(response.cc_definitions_size());
   for (const auto& ccDef : response.cc_definitions()) {
      try {
         ccDefs.push_back({ ccDef.security_id(), ccDef.product()
            , bs::Address::fromAddressString(ccDef.genesis_address())
            , ccDef.lot_size() });
      }
      catch (const std::exception& e) {
         logger_->error("[{}] failed to decode CC definition: {}", __func__, e.what());
      }
   }
   ccFileMgr_->SetLoadedDefinitions(ccDefs);
   return true;
}

bool AssetsAdapter::onMatchingLogin(const MatchingMessage_LoggedIn&)
{
   MatchingMessage msg;
   msg.mutable_get_submitted_auth_addresses();
   Envelope env{ 0, user_, UserTerminal::create(TerminalUsers::Matching)
      , {}, {}, msg.SerializeAsString(), true };
   return pushFill(env);
}

bool AssetsAdapter::processSubmittedAuth(const MatchingMessage_SubmittedAuthAddresses& response)
{
   AssetsMessage msg;
   auto msgBC = msg.mutable_submitted_auth_addrs();
   for (const auto& addr : response.addresses()) {
      msgBC->add_addresses(addr);
   }
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   return pushFill(env);
}

bool AssetsAdapter::processSubmitAuth(const std::string& address)
{  // currently using Celer storage for this, but this might changed at some point
   MatchingMessage msg;
   msg.set_submit_auth_address(address);
   Envelope env{ 0, user_, UserTerminal::create(TerminalUsers::Matching)
      , {}, {}, msg.SerializeAsString(), true };
   return pushFill(env);
}

bool AssetsAdapter::processBalance(const std::string& currency, double value)
{
   AssetsMessage msg;
   auto msgBal = msg.mutable_balance();
   msgBal->set_currency(currency);
   msgBal->set_value(value);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   return pushFill(env);
}
