/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MktDataAdapter.h"
#include <spdlog/spdlog.h>
#include "BSMarketDataProvider.h"
#include "ConnectionManager.h"
#include "PubKeyLoader.h"
#include "SslCaBundle.h"
#include "TerminalMessage.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


MktDataAdapter::MktDataAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::MktData))
{
   auto connMgr = std::make_shared<ConnectionManager>(logger_);
   connMgr->setCaBundle(bs::caBundlePtr(), bs::caBundleSize());
   mdProvider_ = std::make_shared<BSMarketDataProvider>(connMgr, logger_, this
      , true, false);
}

bool MktDataAdapter::process(const bs::message::Envelope &env)
{
   if (env.receiver->value() == user_->value()) {
      MktDataMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own request #{}"
            , __func__, env.foreignId());
         return true;
      }
      switch (msg.data_case()) {
      case MktDataMessage::kStartConnection:
         return processStartConnection(msg.start_connection());
      default:
         logger_->warn("[{}] unknown md request {}", __func__, msg.data_case());
         break;
      }
   }
   return true;
}

bool MktDataAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}"
            , __func__, env.foreignId());
         return false;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         AdministrativeMessage admMsg;
         admMsg.set_component_loading(user_->value());
         pushBroadcast(UserTerminal::create(TerminalUsers::System)
            , admMsg.SerializeAsString());
         return true;
      }
   }
   return false;
}

void MktDataAdapter::userWantsToConnect()
{
   logger_->debug("[{}]", __func__);
}

void MktDataAdapter::waitingForConnectionDetails()
{
   logger_->debug("[{}]", __func__);
}

void MktDataAdapter::connected()
{
   connected_ = true;
   MktDataMessage msg;
   msg.mutable_connected();
   pushBroadcast(user_, msg.SerializeAsString());
}

void MktDataAdapter::disconnected()
{
   connected_ = false;
   MktDataMessage msg;
   msg.mutable_disconnected();
   pushBroadcast(user_, msg.SerializeAsString());
}

void MktDataAdapter::onMDUpdate(bs::network::Asset::Type at, const std::string& name
   , bs::network::MDFields fields)
{
   MktDataMessage msg;
   auto msgPrices = msg.mutable_price_update();
   auto msgSecurity = msgPrices->mutable_security();
   msgSecurity->set_name(name);
   msgSecurity->set_asset_type((int)at);
   for (const auto& field : fields) {
      switch (field.type) {
      case bs::network::MDField::PriceOffer:
         msgPrices->set_ask(field.value);
         break;
      case bs::network::MDField::PriceBid:
         msgPrices->set_bid(field.value);
         break;
      case bs::network::MDField::PriceLast:
         msgPrices->set_last(field.value);
         break;
      case bs::network::MDField::DailyVolume:
         msgPrices->set_volume(field.value);
         break;
      case bs::network::MDField::MDTimestamp:
         msgPrices->set_timestamp((uint64_t)field.value);
         break;
      default: break;
      }
   }
   pushBroadcast(user_, msg.SerializeAsString(), true);
}

void MktDataAdapter::onMDSecurityReceived(const std::string& name, const bs::network::SecurityDef& sd)
{
   MktDataMessage msg;
   auto msgBC = msg.mutable_new_security();
   msgBC->set_name(name);
   msgBC->set_asset_type((int)sd.assetType);
   pushBroadcast(user_, msg.SerializeAsString());
}

void MktDataAdapter::allSecuritiesReceived()
{
   MktDataMessage msg;
   auto msgBC = msg.mutable_all_instruments_received();
   pushBroadcast(user_, msg.SerializeAsString());
}

void MktDataAdapter::onNewFXTrade(const bs::network::NewTrade& trade)
{
   sendTrade(trade);
}

void MktDataAdapter::onNewXBTTrade(const bs::network::NewTrade& trade)
{
   sendTrade(trade);
}

void MktDataAdapter::onNewPMTrade(const bs::network::NewPMTrade& trade)
{
   MktDataMessage msg;
   auto msgTrade = msg.mutable_trade();
   msgTrade->set_product(trade.product);
   msgTrade->set_price(trade.price);
   msgTrade->set_amount(trade.amount);
   msgTrade->set_timestamp(trade.timestamp);
   pushBroadcast(user_, msg.SerializeAsString(), true);
}

void MktDataAdapter::sendTrade(const bs::network::NewTrade& trade)
{
   MktDataMessage msg;
   auto msgTrade = msg.mutable_trade();
   msgTrade->set_product(trade.product);
   msgTrade->set_price(trade.price);
   msgTrade->set_amount(trade.amount);
   msgTrade->set_timestamp(trade.timestamp);
   pushBroadcast(user_, msg.SerializeAsString(), true);
}

bool MktDataAdapter::processStartConnection(int e)
{
   if (connected_) {
      logger_->debug("[{}] already connected", __func__);
      return true;
   }
   const auto env = static_cast<ApplicationSettings::EnvConfiguration>(e);
   mdProvider_->SetConnectionSettings(PubKeyLoader::serverHostName(PubKeyLoader::KeyType::MdServer, env)
      , PubKeyLoader::serverHttpsPort());
   mdProvider_->MDLicenseAccepted();
   return true;
}
