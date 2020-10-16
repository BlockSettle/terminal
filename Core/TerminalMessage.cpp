/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <spdlog/spdlog.h>
#include "TerminalMessage.h"
#include "Message/Adapter.h"

#include "terminal.pb.h"

using namespace bs::message;
using namespace BlockSettle::Terminal;

static const std::map<int, std::string> kTerminalUsersMapping = {
   { static_cast<int>(TerminalUsers::BROADCAST), "Broadcast" },
   { static_cast<int>(TerminalUsers::Signer), "Signer  " },
   { static_cast<int>(TerminalUsers::API), "API     " },
   { static_cast<int>(TerminalUsers::Settings), "Settings" },
   { static_cast<int>(TerminalUsers::BsServer), "BsServer" },
   { static_cast<int>(TerminalUsers::Matching), "Matching" },
   { static_cast<int>(TerminalUsers::Assets), "Assets  " },
   { static_cast<int>(TerminalUsers::MktData), "MarketData" },
   { static_cast<int>(TerminalUsers::MDHistory), "MDHistory" },
   { static_cast<int>(TerminalUsers::Blockchain), "Blockchain" },
   { static_cast<int>(TerminalUsers::Wallets), "Wallets" },
   { static_cast<int>(TerminalUsers::OnChainTracker), "OnChainTrk" },
   { static_cast<int>(TerminalUsers::Settlement), "Settlement" },
   { static_cast<int>(TerminalUsers::Chat), "Chat   " }
};

std::string UserTerminal::name() const
{
   const auto itAcc = kTerminalUsersMapping.find(value());
   return (itAcc == kTerminalUsersMapping.end())
      ? User::name() : itAcc->second;
}


TerminalInprocBus::TerminalInprocBus(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{  // we can create multiple queues if needed and distribute them on adapters
   queue_ = std::make_shared<Queue>(std::make_shared<Router>(logger), logger
      , "Main", kTerminalUsersMapping);
}

TerminalInprocBus::~TerminalInprocBus()
{
   shutdown();
}

void TerminalInprocBus::addAdapter(const std::shared_ptr<Adapter> &adapter)
{
   queue_->bindAdapter(adapter);
   adapter->setQueue(queue_);
   const auto &runner = std::dynamic_pointer_cast<bs::MainLoopRuner>(adapter);
   if (runner) {
      runnableAdapter_ = runner;
   }

   static const auto &adminUser = UserTerminal::create(TerminalUsers::System);
   for (const auto &user : adapter->supportedReceivers()) {
      AdministrativeMessage msg;
      msg.set_component_created(user->value());
      bs::message::Envelope env{ 0, adminUser, {}, {}, {}, msg.SerializeAsString() };
      queue_->pushFill(env);
   }
}

void TerminalInprocBus::start()
{
   static const auto &adminUser = UserTerminal::create(TerminalUsers::System);
   AdministrativeMessage msg;
   msg.mutable_start();
   bs::message::Envelope env{ 0, adminUser, {}, {}, {}, msg.SerializeAsString() };
   queue_->pushFill(env);
}

void TerminalInprocBus::shutdown()
{
   runnableAdapter_.reset();
   queue_->terminate();
}

bool TerminalInprocBus::run(int &argc, char **argv)
{
   start();
   if (!runnableAdapter_) {
      return false;
   }
   runnableAdapter_->run(argc, argv);
   return true;
}


//TODO: move to another source file
using namespace BlockSettle::Terminal;
bs::network::Quote bs::message::fromMsg(const MatchingMessage_Quote& msg)
{
   bs::network::Quote quote;
   quote.requestId = msg.request_id();
   quote.quoteId = msg.quote_id();
   quote.security = msg.security();
   quote.product = msg.product();
   quote.price = msg.price();
   quote.quantity = msg.quantity();
   quote.side = msg.buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   quote.assetType = static_cast<bs::network::Asset::Type>(msg.asset_type());
   quote.quotingType = static_cast<bs::network::Quote::QuotingType>(msg.quoting_type());
   quote.requestorAuthPublicKey = msg.req_auth_pub_key();
   quote.dealerAuthPublicKey = msg.deal_auth_pub_key();
   quote.settlementId = msg.settlement_id();
   quote.dealerTransaction = msg.dealer_tx();
   quote.expirationTime = QDateTime::fromSecsSinceEpoch(msg.expiration_time());
   quote.timeSkewMs = msg.time_skew_ms();
   quote.celerTimestamp = msg.timestamp();
   return quote;
}

void bs::message::toMsg(const bs::network::Quote& quote, MatchingMessage_Quote* msg)
{
   msg->set_request_id(quote.requestId);
   msg->set_quote_id(quote.quoteId);
   msg->set_security(quote.security);
   msg->set_product(quote.product);
   msg->set_price(quote.price);
   msg->set_quantity(quote.quantity);
   msg->set_buy(quote.side == bs::network::Side::Buy);
   msg->set_asset_type((int)quote.assetType);
   msg->set_quoting_type((int)quote.quotingType);
   msg->set_req_auth_pub_key(quote.requestorAuthPublicKey);
   msg->set_deal_auth_pub_key(quote.dealerAuthPublicKey);
   msg->set_settlement_id(quote.settlementId);
   msg->set_dealer_tx(quote.dealerTransaction);
   msg->set_expiration_time(quote.expirationTime.toSecsSinceEpoch());
   msg->set_time_skew_ms(quote.timeSkewMs);
   msg->set_timestamp(quote.celerTimestamp);
}
