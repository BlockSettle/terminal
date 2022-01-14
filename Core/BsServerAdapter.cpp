/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
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

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


BsServerAdapter::BsServerAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<UserTerminal>(TerminalUsers::BsServer))
   , userSettl_(std::make_shared<UserTerminal>(TerminalUsers::Settlement))
   , userMtch_(std::make_shared<UserTerminal>(TerminalUsers::Matching))
   , userSettings_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
{
   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   connMgr_->setCaBundle(bs::caBundlePtr(), bs::caBundleSize());
}

bool BsServerAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings message #{}", __func__
            , env.foreignId());
         return true;
      }
      if (msg.data_case() == SettingsMessage::kGetResponse) {
         return processLocalSettings(msg.get_response());
      }
   }
   else if (env.receiver->value() == user_->value()) {
      return processOwnRequest(env);
   }
   return true;
}

bool BsServerAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}"
            , __func__, env.foreignId());
         return false;
      }
      switch (msg.data_case()) {
      case AdministrativeMessage::kStart:
      case AdministrativeMessage::kRestart:
         start();
         return true;
      default: break;
      }
   }
   return false;
}

void BsServerAdapter::start()
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   auto req = msgReq->add_requests();
   req->set_source(SettingSource_Local);
   req->set_type(SettingType_Int);
   req->set_index(SetIdx_Environment);
   pushRequest(user_, UserTerminal::create(TerminalUsers::Settings)
      , msg.SerializeAsString());
}

bool BsServerAdapter::processOwnRequest(const Envelope &env)
{
   BsServerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse own request #{}", __func__, env.foreignId());
      return true;
   }
   switch (msg.data_case()) {
   case BsServerMessage::kOpenConnection:
      return processOpenConnection();
   case BsServerMessage::kCloseConnection:
#if 0
      bsClient_.reset();
#endif
      break;
   case BsServerMessage::kStartLogin:
      return processStartLogin(msg.start_login());
   case BsServerMessage::kCancelLastLogin:
      return processCancelLogin();
   case BsServerMessage::kPubNewKeyResponse:
      return processPuBKeyResponse(msg.pub_new_key_response());
   case BsServerMessage::kTimeout:
      return processTimeout(msg.timeout());
#if 0
   case BsServerMessage::kSendMatching:
      if (bsClient_) {
         bsClient_->celerSend(static_cast<CelerAPI::CelerMessageType>(msg.send_matching().message_type())
            , msg.send_matching().data());
      }
      break;
   case BsServerMessage::kSubmitAuthAddress:
      return processSubmitAuthAddr(env, msg.submit_auth_address());
   case BsServerMessage::kSendUnsignedPayin:
      return processOutUnsignedPayin(msg.send_unsigned_payin());
   case BsServerMessage::kSendSignedPayin:
      return processOutSignedPayin(msg.send_signed_payin());
   case BsServerMessage::kSendSignedPayout:
      return processOutSignedPayout(msg.send_signed_payout());
#endif
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
            pushBroadcast(UserTerminal::create(TerminalUsers::System)
               , admMsg.SerializeAsString());
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
   return true;
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
   return true;
}

bool BsServerAdapter::processOpenConnection()
{
   if (connected_) {
      logger_->error("[{}] already connected", __func__);
      return true;
   }
#if 0
   bsClient_ = std::make_unique<BsClient>(logger_, this);
#endif
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
      pushBroadcast(user_, msg.SerializeAsString(), true);
   });

   auto wsConnection = std::make_unique<WsDataConnection>(logger_, WsDataConnectionParams{});
   auto connection = std::make_unique<Bip15xDataConnection>(logger_, std::move(wsConnection), bip15xTransport);
#if 0
   if (!connection->openConnection(PubKeyLoader::serverHostName(PubKeyLoader::KeyType::Proxy, envConfig_)
      , PubKeyLoader::serverHttpPort(), bsClient_.get())) {
      logger_->error("[{}] failed to set up connection to {}", __func__
         , PubKeyLoader::serverHostName(PubKeyLoader::KeyType::Proxy, envConfig_));
      return false;  //TODO: send negative result response, maybe?
   }
   bsClient_->setConnection(std::move(connection));
#endif
   return true;
}

bool BsServerAdapter::processStartLogin(const std::string& login)
{
   if (!connected_) {
      return false;  // wait for connection to complete
   }
   if (currentLogin_.empty()) {
      currentLogin_ = login;
#if 0
      bsClient_->startLogin(login);
#endif
   }
   else {
      //onStartLoginDone(true, {});
   }
   return true;
}

bool BsServerAdapter::processCancelLogin()
{
   if (currentLogin_.empty()) {
      logger_->warn("[BsServerAdapter::processCancelLogin] no login started - ignoring request");
      return true;
   }
#if 0
   bsClient_->cancelLogin();
#endif
   return true;
}

#if 0
void BsServerAdapter::startTimer(std::chrono::milliseconds timeout
   , const std::function<void()>&cb)
{
   BsServerMessage msg;
   const auto& toKey = CryptoPRNG::generateRandom(4).toHexStr();
   timeouts_[toKey] = cb;
   msg.set_timeout(toKey);
   const auto& timeNow = bs::message::bus_clock::now();
   pushRequest(user_, user_, msg.SerializeAsString(), timeNow + timeout);
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
   pushBroadcast(user_, msg.SerializeAsString());

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
   pushBroadcast(user_, msg.SerializeAsString());

   SettingsMessage msgSet;
   msgSet.set_load_bootstrap(result.bootstrapDataSigned);
   pushRequest(user_, userSettings_, msgSet.SerializeAsString());

   MatchingMessage msgMtch;
   auto msgReq = msgMtch.mutable_login();
   msgReq->set_matching_login(result.celerLogin);
   msgReq->set_terminal_login(currentLogin_);
   pushRequest(user_, userMtch_, msgMtch.SerializeAsString());

   WalletsMessage msgWlt;
   msgWlt.set_set_settlement_fee(result.feeRatePb);
   pushRequest(user_, userWallets_, msgWlt.SerializeAsString());

   //TODO: send chat login
   currentLogin_.clear();
}

void BsServerAdapter::onProcessPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response& response)
{
   switch (response.data_case()) {
   case Blocksettle::Communication::ProxyTerminalPb::Response::kUpdateOrders:
      processUpdateOrders(response.update_orders());
      break;
   case Blocksettle::Communication::ProxyTerminalPb::Response::kSendUnsignedPayin:
      processUnsignedPayin(response.send_unsigned_payin());
      break;
   case Blocksettle::Communication::ProxyTerminalPb::Response::kSignPayout:
      processSignPayout(response.sign_payout());
      break;
   case Blocksettle::Communication::ProxyTerminalPb::Response::kSignPayin:
      processSignPayin(response.sign_payin());
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
   pushBroadcast(user_, msg.SerializeAsString());
}

void BsServerAdapter::Disconnected()
{
   connected_ = false;
   BsServerMessage msg;
   msg.mutable_disconnected();
   pushBroadcast(user_, msg.SerializeAsString());
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
   pushBroadcast(user_, msg.SerializeAsString());
}

void BsServerAdapter::onTradingStatusChanged(bool tradingEnabled)
{
   BsServerMessage msg;
   msg.set_trading_enabled(tradingEnabled);
   pushBroadcast(user_, msg.SerializeAsString());
}
#endif
