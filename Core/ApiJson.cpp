/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ApiJson.h"
#include <spdlog/spdlog.h>
#include "Address.h"
#include "BsClient.h"
#include "MessageUtils.h"
#include "ProtobufUtils.h"
#include "SslServerConnection.h"
#include "StringUtils.h"

#include "common.pb.h"
#include "json.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace BlockSettle::API::JSON;
using namespace bs::message;
using namespace ProtobufUtils;

static constexpr auto kRequestTimeout = std::chrono::seconds{ 90 };


ApiJsonAdapter::ApiJsonAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , userSettings_(UserTerminal::create(TerminalUsers::Settings))
{}

bool ApiJsonAdapter::process(const Envelope &env)
{
   if (std::dynamic_pointer_cast<UserTerminal>(env.sender)) {
      switch (env.sender->value<bs::message::TerminalUsers>()) {
      case TerminalUsers::System:
         return processAdminMessage(env);
      case TerminalUsers::Settings:
         return processSettings(env);
      case TerminalUsers::Blockchain:
         return processBlockchain(env);
      case TerminalUsers::Signer:
         return processSigner(env);
      case TerminalUsers::Wallets:
         return processWallets(env);
      case TerminalUsers::BsServer:
         return processBsServer(env);
      case TerminalUsers::Settlement:
         return processSettlement(env);
      case TerminalUsers::Matching:
         return processMatching(env);
      case TerminalUsers::MktData:
         return processMktData(env);
      case TerminalUsers::OnChainTracker:
         return processOnChainTrack(env);
      case TerminalUsers::Assets:
         return processAssets(env);
      default:    break;
      }
   }
   else if (env.receiver && (env.sender->value() == user_->value())
      && (env.sender->value() == env.receiver->value())) {  // own to self messages
      if (env.message == "GC") {
         processGCtimeout();
      }
   }
   else {
      logger_->warn("[{}] non-terminal #{} user {}", __func__, env.id
         , env.sender->name());
   }
   return true;
}

static std::shared_ptr<UserTerminal> mapUser(const EnvelopeIn::MessageCase& user)
{
   static const std::map<EnvelopeIn::MessageCase, std::shared_ptr<UserTerminal>> usersMap = {
      { EnvelopeIn::kSigner, UserTerminal::create(TerminalUsers::Signer) },
      { EnvelopeIn::kMatching, UserTerminal::create(TerminalUsers::Matching) },
      { EnvelopeIn::kAssets, UserTerminal::create(TerminalUsers::Assets) },
      { EnvelopeIn::kMarketData, UserTerminal::create(TerminalUsers::MktData) },
      { EnvelopeIn::kMdHist, UserTerminal::create(TerminalUsers::MDHistory) },
      { EnvelopeIn::kBlockchain, UserTerminal::create(TerminalUsers::Blockchain) },
      { EnvelopeIn::kWallets, UserTerminal::create(TerminalUsers::Wallets) },
      { EnvelopeIn::kOnChainTracker, UserTerminal::create(TerminalUsers::OnChainTracker) },
      { EnvelopeIn::kSettlement, UserTerminal::create(TerminalUsers::Settlement) },
      { EnvelopeIn::kChat, UserTerminal::create(TerminalUsers::Chat) },
      { EnvelopeIn::kBsServer, UserTerminal::create(TerminalUsers::BsServer) }
   };
   try {
      return usersMap.at(user);
   }
   catch (const std::exception&) {
      return {};
   }
}

void ApiJsonAdapter::OnDataFromClient(const std::string& clientId
   , const std::string& data)
{
   const auto& sendErrorReply = [this, clientId]
      (const std::string& id, const std::string& errorMsg)
   {
      EnvelopeOut env;
      if (!id.empty()) {
         env.set_id(id);
      }
      auto msg = env.mutable_error();
      if (!errorMsg.empty()) {
         msg->set_error_text(errorMsg);
      }
      const auto& jsonData = toJsonCompact(env);
      if (!connection_->SendDataToClient(clientId, jsonData)) {
         logger_->error("[ApiJsonAdapter::OnDataFromClient::sendErrorReply] failed to send");
      }
   };
   logger_->debug("[{}] received from {}:\n{}", __func__, bs::toHex(clientId), data);
   EnvelopeIn jsonMsg;
   if (!fromJson(data, &jsonMsg)) {
      sendErrorReply({}, "failed to parse");
      return;
   }
   std::string serMsg;
   switch (jsonMsg.message_case()) {
   case EnvelopeIn::kSigner:
      serMsg = jsonMsg.signer().SerializeAsString();
      break;
   case EnvelopeIn::kMatching:
      serMsg = jsonMsg.matching().SerializeAsString();
      break;
   case EnvelopeIn::kAssets:
      serMsg = jsonMsg.assets().SerializeAsString();
      break;
   case EnvelopeIn::kMarketData:
      serMsg = jsonMsg.market_data().SerializeAsString();
      break;
   case EnvelopeIn::kMdHist:
      serMsg = jsonMsg.md_hist().SerializeAsString();
      break;
   case EnvelopeIn::kBlockchain:
      serMsg = jsonMsg.blockchain().SerializeAsString();
      break;
   case EnvelopeIn::kWallets:
      serMsg = jsonMsg.wallets().SerializeAsString();
      break;
   case EnvelopeIn::kOnChainTracker:
      serMsg = jsonMsg.on_chain_tracker().SerializeAsString();
      break;
   case EnvelopeIn::kSettlement:
      serMsg = jsonMsg.settlement().SerializeAsString();
      break;
   case EnvelopeIn::kChat:
      serMsg = jsonMsg.chat().SerializeAsString();
      break;
   case EnvelopeIn::kBsServer:
      serMsg = jsonMsg.bs_server().SerializeAsString();
      break;
   default:
      logger_->warn("[{}] unknown message for {}", __func__, jsonMsg.message_case());
      break;
   }
   const auto& user = mapUser(jsonMsg.message_case());
   if (!user) {
      logger_->error("[{}] failed to map user from {}", __func__
         , jsonMsg.message_case());
      return;
   }
   Envelope env{ 0, user_, user, {}, {}, serMsg, true };
   if (!pushFill(env)) {
      sendErrorReply(jsonMsg.id(), "internal error");
   }
   requests_[env.id] = { clientId, jsonMsg.id(), std::chrono::system_clock::now() };
}

void ApiJsonAdapter::OnClientConnected(const std::string& clientId
   , const Details& details)
{
   connectedClients_.insert(clientId);
   logger_->info("[{}] {} (total {}) connected from {}", __func__, bs::toHex(clientId)
      , connectedClients_.size(), details.at(Detail::IpAddr));
   EnvelopeOut env;
   auto msg = env.mutable_connected();
   msg->set_wallets_ready(walletsReady_);
   msg->set_logged_user(loggedInUser_);
   msg->set_matching_connected(matchingConnected_);
   msg->set_blockchain_state(armoryState_);
   msg->set_top_block(blockNum_);
   msg->set_signer_state(signerState_);
   const auto& jsonData = toJsonCompact(env);
   if (!connection_->SendDataToClient(clientId, jsonData)) {
      logger_->error("[ApiJsonAdapter::OnClientConnected] failed to send");
   }
   if (connectedClients_.size() == 1) {
      sendGCtimeout();
   }
}

void ApiJsonAdapter::OnClientDisconnected(const std::string& clientId)
{
   connectedClients_.erase(clientId);
   logger_->info("[{}] {} disconnected ({} remain)", __func__, bs::toHex(clientId)
      , connectedClients_.size());
}

bool ApiJsonAdapter::processSettings(const Envelope &env)
{
   SettingsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse settings msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case SettingsMessage::kGetResponse:
      return processSettingsGetResponse(msg.get_response());
   case SettingsMessage::kSettingsUpdated:
      return processSettingsGetResponse(msg.settings_updated());
   case SettingsMessage::kApiPrivkey:
      connectionPrivKey_.assign(msg.api_privkey().cbegin(), msg.api_privkey().cend());
      connection_ = std::make_unique<SslServerConnection>(logger_
         , SslServerConnectionParams{ true, false, true
         , bs::network::ws::generateSelfSignedCert(connectionPrivKey_)
         , connectionPrivKey_, [this](const std::string& publicKey) -> bool
         {
            if (clientPubKeys_.empty()) {  // no client keys configured
               return true;      // TODO: change this if unknown clients are forbidden
            }
            if (clientPubKeys_.find(publicKey) == clientPubKeys_.end()) {
               logger_->error("[ApiJsonAdapter] unknown client key {}"
                  , bs::toHex(publicKey));
               return false;
            }
            return true;
         } }
      );
      break;
   case SettingsMessage::kApiClientKeys:
      for (const auto& clientKey : msg.api_client_keys().pub_keys()) {
         clientPubKeys_.insert(clientKey);
      }
      break;
   default: break;
   }
   return true;
}

bool ApiJsonAdapter::processSettingsGetResponse(const SettingsMessage_SettingsResponse &response)
{
   std::map<int, QVariant> settings;
   for (const auto &setting : response.responses()) {
      switch (setting.request().index()) {
      case SetIdx_ExtConnPort:
         if (!connection_) {
            SPDLOG_LOGGER_ERROR(logger_, "connection should be created at this point");
            break;
         }
         if (connection_->BindConnection("0.0.0.0", setting.s(), this)) {
            logger_->debug("[ApiJsonAdapter] connection ready on port {}", setting.s());
         }
         else {
            SPDLOG_LOGGER_ERROR(logger_, "failed to bind to {}", setting.s());
         }
         break;
      }
   }
   return true;
}

bool ApiJsonAdapter::processAdminMessage(const Envelope &env)
{
   AdministrativeMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse admin msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case AdministrativeMessage::kStart:
      processStart();
      break;
   default: break;
   }
   return true;
}

bool ApiJsonAdapter::processBlockchain(const Envelope &env)
{
   ArmoryMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processBlockchain] failed to parse msg #{}"
         , env.id);
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return true;
   }
   switch (msg.data_case()) {
   case ArmoryMessage::kStateChanged:
      armoryState_ = msg.state_changed().state();
      blockNum_ = msg.state_changed().top_block();
      sendReplyToClient(0, msg, env.sender);
      break;
   case ArmoryMessage::kNewBlock:
      blockNum_ = msg.new_block().top_block();
      sendReplyToClient(0, msg, env.sender);
      break;
   case ArmoryMessage::kWalletRegistered:
      if (msg.wallet_registered().success() && msg.wallet_registered().wallet_id().empty()) {
         walletsReady_ = true;
      }
      break;
   case ArmoryMessage::kLedgerEntries: [[fallthrough]];
   case ArmoryMessage::kAddressHistory: [[fallthrough]];
   case ArmoryMessage::kFeeLevelsResponse:
      if (hasRequest(env.id)) {
         sendReplyToClient(env.id, msg, env.sender);
      }
      break;
   case ArmoryMessage::kZcReceived: [[fallthrough]];
   case ArmoryMessage::kZcInvalidated:
      sendReplyToClient(0, msg, env.sender);
      break;
   default:    break;
   }
   return true;
}

bool ApiJsonAdapter::processSigner(const Envelope &env)
{
   SignerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processSigner] failed to parse msg #{}"
         , env.id);
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return true;
   }
   switch (msg.data_case()) {
   case SignerMessage::kState:
      signerState_ = msg.state().code();
      sendReplyToClient(0, msg, env.sender);
      break;
   case SignerMessage::kNeedNewWalletPrompt:
      break;
   case SignerMessage::kSignTxResponse:
      sendReplyToClient(env.id, msg, env.sender);
      break;
   default:    break;
   }
   return true;
}

bool ApiJsonAdapter::processWallets(const Envelope &env)
{
   WalletsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case WalletsMessage::kWalletLoaded: [[fallthrough]];
   case WalletsMessage::kAuthWallet: [[fallthrough]];
   case WalletsMessage::kHdWallet:
      sendReplyToClient(0, msg, env.sender);
      break;
   case WalletsMessage::kWalletAddresses: [[fallthrough]];
   case WalletsMessage::kAddrComments: [[fallthrough]];
   case WalletsMessage::kWalletData: [[fallthrough]];
   case WalletsMessage::kWalletBalances: [[fallthrough]];
   case WalletsMessage::kTxDetailsResponse: [[fallthrough]];
   case WalletsMessage::kWalletsListResponse: [[fallthrough]];
   case WalletsMessage::kUtxos: [[fallthrough]];
   case WalletsMessage::kAuthKey: [[fallthrough]];
   case WalletsMessage::kReservedUtxos:
      if (hasRequest(env.id)) {
         sendReplyToClient(env.id, msg, env.sender);
      }
      break;
   default:    break;
   }
   return true;
}

bool ApiJsonAdapter::processOnChainTrack(const Envelope &env)
{
   OnChainTrackMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case OnChainTrackMessage::kAuthState: [[fallthrough]];
   case OnChainTrackMessage::kVerifiedAuthAddresses:
      sendReplyToClient(0, msg, env.sender);
      break;
   default:    break;
   }
   return true;
}

bool ApiJsonAdapter::processAssets(const bs::message::Envelope& env)
{
   AssetsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case AssetsMessage::kSubmittedAuthAddrs:
      if (hasRequest(env.id)) {
         sendReplyToClient(env.id, msg, env.sender);
      }
      break;
   case AssetsMessage::kBalance:
      sendReplyToClient(0, msg, env.sender);
      break;
   default: break;
   }
   return true;
}

void ApiJsonAdapter::processStart()
{
   logger_->debug("[{}]", __func__);
   SettingsMessage msg;
   msg.set_api_privkey("");
   Envelope envReq1{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(envReq1);

   msg.mutable_api_client_keys();
   Envelope envReq2{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(envReq2);

   auto msgReq = msg.mutable_get_request();
   auto setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_ExtConnPort);
   setReq->set_type(SettingType_String);
   Envelope envReq3{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(envReq3);
}

bool ApiJsonAdapter::processBsServer(const bs::message::Envelope& env)
{
   BsServerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case BsServerMessage::kStartLoginResult: [[fallback]]
   case BsServerMessage::kOrdersUpdate:
      sendReplyToClient(0, msg, env.sender); // multicast to all connected clients
      break;
   case BsServerMessage::kLoginResult:
      loggedInUser_ = msg.login_result().login();
      sendReplyToClient(0, msg, env.sender); // multicast login results to all connected clients
      break;
   case BsServerMessage::kDisconnected:
      loggedInUser_.clear();
      sendReplyToClient(0, msg, env.sender);
      break;
   default: break;
   }
   return true;
}

bool ApiJsonAdapter::processSettlement(const bs::message::Envelope& env)
{
   SettlementMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case SettlementMessage::kQuote: [[fallthrough]];
   case SettlementMessage::kMatchedQuote: [[fallthrough]];
   case SettlementMessage::kFailedSettlement: [[fallthrough]];
   case SettlementMessage::kPendingSettlement: [[fallthrough]];
   case SettlementMessage::kSettlementComplete: [[fallthrough]];
   case SettlementMessage::kQuoteCancelled: [[fallthrough]];
   case SettlementMessage::kQuoteReqNotif:
      sendReplyToClient(0, msg, env.sender);
      break;
   default:    break;
   }
   return true;
}

bool ApiJsonAdapter::processMatching(const bs::message::Envelope& env)
{
   MatchingMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case MatchingMessage::kLoggedIn:
      matchingConnected_ = true;
      sendReplyToClient(0, msg, env.sender);
      break;
   case MatchingMessage::kLoggedOut:
      matchingConnected_ = false;
      loggedInUser_.clear();
      sendReplyToClient(0, msg, env.sender);
      break;
   default:    break;
   }
   return true;
}

bool ApiJsonAdapter::processMktData(const bs::message::Envelope& env)
{
   MktDataMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case MktDataMessage::kDisconnected: [[fallthrough]];
   case MktDataMessage::kNewSecurity: [[fallthrough]];
   case MktDataMessage::kAllInstrumentsReceived: [[fallthrough]];
   case MktDataMessage::kPriceUpdate:
      sendReplyToClient(0, msg, env.sender);
      break;
   default: break;
   }
   return true;
}

bool ApiJsonAdapter::hasRequest(uint64_t msgId) const
{
   return (requests_.find(msgId) != requests_.end());
}

bool ApiJsonAdapter::sendReplyToClient(uint64_t msgId
   , const google::protobuf::Message& msg
   , const std::shared_ptr<bs::message::User>& sender)
{
   if (!connection_ || connectedClients_.empty()) {
      return true;
   }
   EnvelopeOut envOut;
   std::string clientId;
   if (msgId) {
      const auto& itReq = requests_.find(msgId);
      if (itReq == requests_.end()) {
         logger_->error("[{}] no request found for #{} from {}; reply:\n{}"
            , __func__, msgId, sender->name(), msg.DebugString());
         return false;
      }
      clientId = itReq->second.clientId;
      envOut.set_id(itReq->second.requestId);
      itReq->second.replied = true;
   }
   google::protobuf::Message* targetMsg = nullptr;
   switch (sender->value<TerminalUsers>()) {
   case TerminalUsers::Signer:
      targetMsg = envOut.mutable_signer();
      break;
   case TerminalUsers::Matching:
      targetMsg = envOut.mutable_matching();
      break;
   case TerminalUsers::Assets:
      targetMsg = envOut.mutable_assets();
      break;
   case TerminalUsers::MktData:
      targetMsg = envOut.mutable_market_data();
      break;
   case TerminalUsers::MDHistory:
      targetMsg = envOut.mutable_md_hist();
      break;
   case TerminalUsers::Blockchain:
      targetMsg = envOut.mutable_blockchain();
      break;
   case TerminalUsers::Wallets:
      targetMsg = envOut.mutable_wallets();
      break;
   case TerminalUsers::OnChainTracker:
      targetMsg = envOut.mutable_on_chain_tracker();
      break;
   case TerminalUsers::Settlement:
      targetMsg = envOut.mutable_settlement();
      break;
   case TerminalUsers::Chat:
      targetMsg = envOut.mutable_chat();
      break;
   case TerminalUsers::BsServer:
      targetMsg = envOut.mutable_bs_server();
      break;
   default:
      logger_->warn("[{}] unhandled sender {}", __func__, sender->value());
      break;
   }
   if (targetMsg) {
      targetMsg->CopyFrom(msg);
   }
   else {
      logger_->error("[{}] unknown target message", __func__);
      return false;
   }
   const auto& jsonData = toJsonCompact(envOut);
   if (clientId.empty()) {
      return connection_->SendDataToAllClients(jsonData);
   }
   else {
      if (connectedClients_.find(clientId) == connectedClients_.end()) {
         logger_->error("[{}] client {} is not connected", __func__, bs::toHex(clientId));
         return false;
      }
      return connection_->SendDataToClient(clientId, jsonData);
   }
}

void ApiJsonAdapter::sendGCtimeout()
{
   const auto& timeNow = std::chrono::system_clock::now();
   Envelope env{ 0, user_, user_, timeNow, timeNow + kRequestTimeout, "GC" };
   pushFill(env);
}

void ApiJsonAdapter::processGCtimeout()
{
   std::vector<uint64_t> deleteRequests;
   const auto& timeNow = std::chrono::system_clock::now();
   for (const auto& req : requests_) {
      if ((timeNow - req.second.timestamp) > kRequestTimeout) {
         if (!req.second.replied) {
            logger_->debug("[{}] request #{}/{} from {} was never replied", __func__
               , req.first, req.second.requestId, bs::toHex(req.second.clientId));
         }
         deleteRequests.push_back(req.first);
      }
   }
   if (!deleteRequests.empty()) {
      logger_->debug("[{}] removing {} outdated request[s]", __func__
         , deleteRequests.size());
      for (const auto& id : deleteRequests) {
         requests_.erase(id);
      }
   }
   if (!connectedClients_.empty()) {
      sendGCtimeout();
   }
}
