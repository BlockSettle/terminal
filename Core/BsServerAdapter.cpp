/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BsServerAdapter.h"
#include <spdlog/spdlog.h>
#include "Bip15xDataConnection.h"
#include "ConnectionManager.h"
#include "PubKeyLoader.h"
#include "RequestReplyCommand.h"
#include "SslCaBundle.h"
#include "TerminalMessage.h"
#include "WsDataConnection.h"

#include "bs_proxy_terminal_pb.pb.h"
#include "bs_communication.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Terminal;
using namespace bs::message;


BsServerAdapter::BsServerAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::BsServer))
{
   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   connMgr_->setCaBundle(bs::caBundlePtr(), bs::caBundleSize());
}

bool BsServerAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case AdministrativeMessage::kStart:
         start();
         break;
      case AdministrativeMessage::kRestart:
         start();
         break;
      }
   }
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings message #{}", __func__, env.id);
         return true;
      }
      if (msg.data_case() == SettingsMessage::kGetResponse) {
         return processLocalSettings(msg.get_response());
      }
   }
   else if (env.receiver && (env.receiver->value() == user_->value())) {
      return processOwnRequest(env);
   }
   return true;
}

void BsServerAdapter::start()
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   auto req = msgReq->add_requests();
   req->set_source(SettingSource_Local);
   req->set_type(SettingType_Int);
   req->set_index(SetIdx_Environment);
   Envelope envReq{ 0, user_, UserTerminal::create(TerminalUsers::Settings)
      , {}, {}, msg.SerializeAsString() };
   pushFill(envReq);
}

bool BsServerAdapter::processOwnRequest(const Envelope &env)
{
   BsServerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse own request #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case BsServerMessage::kOpenConnection:
      return processOpenConnection();
   case BsServerMessage::kCloseConnection:
      bsClient_.reset();
      break;
   case BsServerMessage::kStartLogin:
      return processStartLogin(msg.start_login());
   case BsServerMessage::kCancelLastLogin:
      return processCancelLogin();
   case BsServerMessage::kPubNewKeyResponse:
      return processPuBKeyResponse(msg.pub_new_key_response());
   case BsServerMessage::kTimeout:
      return processTimeout(msg.timeout());
   case BsServerMessage::kSendMatching:
      if (bsClient_) {
         bsClient_->celerSend(static_cast<CelerAPI::CelerMessageType>(msg.send_matching().message_type())
            , msg.send_matching().data());
      }
      break;
   case BsServerMessage::kSubmitAuthAddress:
      return processSubmitAuthAddr(env, msg.submit_auth_address());
   default:    break;
   }
   return true;
}

bool BsServerAdapter::processLocalSettings(const SettingsMessage_SettingsResponse &response)
{
   for (const auto &setting : response.responses()) {
      switch (static_cast<ApplicationSettings::Setting>(setting.request().index())) {
      case ApplicationSettings::envConfiguration:
         envConfig_ = static_cast<ApplicationSettings::EnvConfiguration>(setting.i());
         {
            AdministrativeMessage admMsg;
            admMsg.set_component_loading(user_->value());
            Envelope envBC{ 0, UserTerminal::create(TerminalUsers::System), nullptr
               , {}, {}, admMsg.SerializeAsString() };
            pushFill(envBC);
         }
         break;

      default: break;
      }
   }
   return true;
}

bool BsServerAdapter::processPuBKeyResponse(bool allowed)
{
   if (!futPuBkey_) {
      logger_->error("[{}] not waiting for PuB key permission", __func__);
      return true;
   }
   futPuBkey_->setValue(allowed);
   futPuBkey_.reset();
}

bool BsServerAdapter::processTimeout(const std::string& id)
{
   const auto& itTO = timeouts_.find(id);
   if (itTO == timeouts_.end()) {
      logger_->error("[{}] unknown timeout {}", __func__, id);
      return true;
   }
   itTO->second();
   timeouts_.erase(itTO);
}

bool BsServerAdapter::processOpenConnection()
{
   if (connected_) {
      logger_->error("[{}] already connected", __func__);
      return true;
   }
   bsClient_ = std::make_unique<BsClient>(logger_, this);
   bs::network::BIP15xParams params;
   params.ephemeralPeers = true;
   params.authMode = bs::network::BIP15xAuthMode::OneWay;
   const auto& bip15xTransport = std::make_shared<bs::network::TransportBIP15xClient>(logger_, params);
   bip15xTransport->setKeyCb([this](const std::string & oldKey, const std::string & newKey
      , const std::string & srvAddrPort, const std::shared_ptr<FutureValue<bool>> &prompt) {
      prompt->setValue(true);
      return;  //TODO: remove this and above after implementing GUI support for the code below
      futPuBkey_ = prompt;
      BsServerMessage msg;
      auto msgReq = msg.mutable_pub_new_key_request();
      msgReq->set_old_key(oldKey);
      msgReq->set_new_key(newKey);
      msgReq->set_server_id(srvAddrPort);
      Envelope envBC{ 0, user_, nullptr, {}, {}, msg.SerializeAsString(), true };
      pushFill(envBC);
   });

   auto wsConnection = std::make_unique<WsDataConnection>(logger_, WsDataConnectionParams{});
   auto connection = std::make_unique<Bip15xDataConnection>(logger_, std::move(wsConnection), bip15xTransport);
   if (!connection->openConnection(PubKeyLoader::serverHostName(PubKeyLoader::KeyType::Proxy, envConfig_)
      , PubKeyLoader::serverHttpPort(), bsClient_.get())) {
      logger_->error("[{}] failed to set up connection to {}", __func__
         , PubKeyLoader::serverHostName(PubKeyLoader::KeyType::Proxy, envConfig_));
      return false;  //TODO: send negative result response, maybe?
   }
   bsClient_->setConnection(std::move(connection));
   return true;
}

bool BsServerAdapter::processStartLogin(const std::string& login)
{
   if (!connected_) {
      return false;  // wait for connection to complete
   }
   if (!currentLogin_.empty()) {
      logger_->warn("[{}] can't start before login {} processing is complete"
         , __func__, currentLogin_);
      return false;
   }
   currentLogin_ = login;
   bsClient_->startLogin(login);
   return true;
}

bool BsServerAdapter::processCancelLogin()
{
   if (currentLogin_.empty()) {
      logger_->warn("[BsServerAdapter::processCancelLogin] no login started - ignoring request");
      return true;
   }
   bsClient_->cancelLogin();
   return true;
}

bool BsServerAdapter::processSubmitAuthAddr(const bs::message::Envelope& env
   , const std::string& addr)
{
   const auto& sendReply = [this, env](bs::error::AuthAddressSubmitResult code)
   {
      BsServerMessage msg;
      msg.set_submit_auth_result((int)code);
      Envelope envResp{ env.id, user_, env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(envResp);
   };
   const auto& address = bs::Address::fromAddressString(addr);
   bsClient_->signAuthAddress(address, [this, address, sendReply](const BsClient::SignResponse& response) {
      if (response.userCancelled) {
         logger_->error("[BsServerAdapter::processSubmitAuthAddr] signing auth "
            "address cancelled: {}", response.errorMsg);
         sendReply(bs::error::AuthAddressSubmitResult::AuthSignCancelled);
         return;
      }
      if (!response.success) {
         logger_->error("[BsServerAdapter::processSubmitAuthAddr] signing auth "
            "address failed: {}", response.errorMsg);
         sendReply(bs::error::AuthAddressSubmitResult::AuthRequestSignFailed);
         return;
      }
      logger_->debug("[BsServerAdapter::processSubmitAuthAddr] signing auth address succeed");

      bsClient_->confirmAuthAddress(address, [this, address, sendReply](bs::error::AuthAddressSubmitResult submitResult) {
         sendReply(submitResult);
         if (submitResult != bs::error::AuthAddressSubmitResult::Success) {
            logger_->error("[BsServerAdapter::processSubmitAuthAddr] confirming"
               " auth address failed: {}", static_cast<int>(submitResult));
         }
         else {
            logger_->debug("[BsServerAdapter::processSubmitAuthAddr] confirming"
               " auth address succeed");
         }

         AssetsMessage msg;
         msg.set_submit_auth_address(address.display());
         Envelope envReq{ 0, user_, UserTerminal::create(TerminalUsers::Assets)
            , {}, {}, msg.SerializeAsString(), true };
         pushFill(envReq);
      });
   });
   return true;
}

void BsServerAdapter::processUpdateOrders(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOrders& orders)
{
   BsServerMessage msg;
   auto msgOrders = msg.mutable_orders_update();
   for (const auto& order : orders.orders()) {
      auto msgOrder = msgOrders->add_orders();
      switch (order.status()) {
      case bs::types::ORDER_STATUS_PENDING:
         msgOrder->set_status((int)bs::network::Order::Pending);
         break;
      case bs::types::ORDER_STATUS_FILLED:
         msgOrder->set_status((int)bs::network::Order::Filled);
         break;
      case bs::types::ORDER_STATUS_VOID:
         msgOrder->set_status((int)bs::network::Order::Failed);
         break;
      default:
         break;
      }
      msgOrder->set_status_text(order.status_text());
      msgOrder->set_product(order.product());
      msgOrder->set_contra_product(order.product_against());
      msgOrder->set_buy(order.side() == bs::types::Side::SIDE_BUY);
      msgOrder->set_quantity(order.quantity());
      msgOrder->set_price(order.price());
      msgOrder->set_timestamp(order.timestamp_ms());
   }
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void BsServerAdapter::startTimer(std::chrono::milliseconds timeout
   , const std::function<void()>&cb)
{
   BsServerMessage msg;
   const auto& toKey = CryptoPRNG::generateRandom(4).toHexStr();
   timeouts_[toKey] = cb;
   msg.set_timeout(toKey);
   const auto& timeNow = std::chrono::system_clock::now();
   Envelope env{ 0, user_, user_, timeNow, timeNow + timeout, msg.SerializeAsString(), true };
   pushFill(env);
}

void BsServerAdapter::onStartLoginDone(bool success, const std::string& errorMsg)
{
   if (currentLogin_.empty()) {
      logger_->error("[{}] no pending login", __func__);
      return;
   }
   BsServerMessage msg;
   auto msgResp = msg.mutable_start_login_result();
   msgResp->set_login(currentLogin_);
   msgResp->set_success(success);
   msgResp->set_error_text(errorMsg);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);

   if (success) {
      bsClient_->getLoginResult();
   }
   else {
      currentLogin_.clear();
   }
}

void BsServerAdapter::onGetLoginResultDone(const BsClientLoginResult& result)
{
   if (currentLogin_.empty()) {
      logger_->error("[{}] no pending login", __func__);
      return;
   }
   BsServerMessage msg;
   auto msgResp = msg.mutable_login_result();
   msgResp->set_login(currentLogin_);
   currentLogin_.clear();
   msgResp->set_status((int)result.status);
   msgResp->set_user_type((int)result.userType);
   msgResp->set_error_text(result.errorMsg);
   msgResp->set_celer_login(result.celerLogin);
   msgResp->set_chat_token(result.chatTokenData.toBinStr());
   msgResp->set_chat_token_signature(result.chatTokenSign.toBinStr());
   msgResp->set_bootstrap_signed_data(result.bootstrapDataSigned);
   msgResp->set_enabled(result.enabled);
   msgResp->set_fee_rate(result.feeRatePb);
   auto msgTradeSet = msgResp->mutable_trade_settings();
   msgTradeSet->set_xbt_tier1_limit(result.tradeSettings.xbtTier1Limit);
   msgTradeSet->set_xbt_price_band(result.tradeSettings.xbtPriceBand);
   msgTradeSet->set_auth_reqd_settl_trades(result.tradeSettings.authRequiredSettledTrades);
   msgTradeSet->set_auth_submit_addr_limit(result.tradeSettings.authSubmitAddressLimit);
   msgTradeSet->set_dealer_auth_submit_addr_limit(result.tradeSettings.dealerAuthSubmitAddressLimit);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void BsServerAdapter::onCelerRecv(CelerAPI::CelerMessageType messageType, const std::string& data)
{
   BsServerMessage msg;
   auto msgResp = msg.mutable_recv_matching();
   msgResp->set_message_type((int)messageType);
   msgResp->set_data(data);
   Envelope env{ 0, user_, bs::message::UserTerminal::create(bs::message::TerminalUsers::Matching)
      , {}, {}, msg.SerializeAsString() };   // send directly to matching adapter, not broadcast
   pushFill(env);
}

void BsServerAdapter::onProcessPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response& response)
{
   switch (response.data_case()) {
   case Blocksettle::Communication::ProxyTerminalPb::Response::kUpdateOrders:
      processUpdateOrders(response.update_orders());
      break;
   default:
      break;
   }
}

void BsServerAdapter::Connected()
{
   connected_ = true;
   BsServerMessage msg;
   msg.mutable_connected();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void BsServerAdapter::Disconnected()
{
   connected_ = false;
   BsServerMessage msg;
   msg.mutable_disconnected();
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void BsServerAdapter::onConnectionFailed()
{
   Disconnected();
}

void BsServerAdapter::onBalanceUpdated(const std::string& currency, double balance)
{
   BsServerMessage msg;
   auto msgBal = msg.mutable_balance_updated();
   msgBal->set_currency(currency);
   msgBal->set_value(balance);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

void BsServerAdapter::onTradingStatusChanged(bool tradingEnabled)
{
   BsServerMessage msg;
   msg.set_trading_enabled(tradingEnabled);
   Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}
