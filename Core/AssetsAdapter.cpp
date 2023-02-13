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

#include "common.pb.h"
#include "terminal.pb.h"

using namespace bs::message;
using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;

AssetsAdapter::AssetsAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Assets))
{
   assetMgr_ = std::make_unique<AssetManager>(logger, this);
}

bool AssetsAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings message #{}", __func__, env.foreignId());
         return true;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kGetResponse:
         return processGetSettings(msg.get_response());
      default: break;
      }
   }
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::Matching) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse matching message #{}", __func__, env.foreignId());
         return true;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kSubmittedAuthAddresses:
         return processSubmittedAuth(msg.submitted_auth_addresses());
      default: break;
      }
   }
   else if (env.receiver->value() == user_->value()) {
      AssetsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own message #{}", __func__, env.foreignId());
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

bool AssetsAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}"
            , __func__, env.foreignId());
         return false;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         SettingsMessage msgSet;
         auto msgReq = msgSet.mutable_get_request();
         auto setReq = msgReq->add_requests();
         setReq->set_source(SettingSource_Local);
         setReq->set_index(SetIdx_BlockSettleSignAddress);
         pushRequest(user_, UserTerminal::create(TerminalUsers::Settings)
            , msgSet.SerializeAsString());
      }
   }
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::Matching) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse matching message #{}", __func__, env.foreignId());
         return false;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kLoggedIn:
         return onMatchingLogin(msg.logged_in());
      default: break;
      }
   }
   else if (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::BsServer) {
      BsServerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse BS message #{}", __func__, env.foreignId());
         return false;
      }
      switch (msg.data_case()) {
      case BsServerMessage::kBalanceUpdated:
         return processBalance(msg.balance_updated().currency(), msg.balance_updated().value());
      default: break;
      }
   }
   return false;
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

bool AssetsAdapter::processGetSettings(const SettingsMessage_SettingsResponse& response)
{
   for (const auto& setting : response.responses()) {
      switch (setting.request().index()) {
      default: break;
      }
   }
   return true;
}

bool AssetsAdapter::onMatchingLogin(const MatchingMessage_LoggedIn&)
{
   MatchingMessage msg;
   msg.mutable_get_submitted_auth_addresses();
   return pushRequest(user_, UserTerminal::create(TerminalUsers::Matching)
      , msg.SerializeAsString());
}

bool AssetsAdapter::processSubmittedAuth(const MatchingMessage_SubmittedAuthAddresses& response)
{
   AssetsMessage msg;
   auto msgBC = msg.mutable_submitted_auth_addrs();
   for (const auto& addr : response.addresses()) {
      msgBC->add_addresses(addr);
   }
   return pushBroadcast(user_, msg.SerializeAsString());
}

bool AssetsAdapter::processSubmitAuth(const std::string& address)
{  // currently using Celer storage for this, but this might changed at some point
   MatchingMessage msg;
   msg.set_submit_auth_address(address);
   return pushRequest(user_, UserTerminal::create(TerminalUsers::Matching)
      , msg.SerializeAsString());
}

bool AssetsAdapter::processBalance(const std::string& currency, double value)
{
   AssetsMessage msg;
   auto msgBal = msg.mutable_balance();
   msgBal->set_currency(currency);
   msgBal->set_value(value);
   return pushBroadcast(user_, msg.SerializeAsString());
}
