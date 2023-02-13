/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MockTerminal.h"
#include "Adapters/BlockchainAdapter.h"
#include "Adapters/WalletsAdapter.h"
#include "ArmorySettings.h"
#include "Message/Adapter.h"
#include "SettingsAdapter.h"
#include "SignerAdapter.h"
#include "TerminalMessage.h"
#include "TestEnv.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;

MockTerminalBus::MockTerminalBus(const std::shared_ptr<spdlog::logger>& logger
   , const std::string &name)
   : logger_(logger)
{
   queue_ = std::make_shared<Queue>(std::make_shared<Router>(logger)
      , logger, name);
}

MockTerminalBus::~MockTerminalBus()
{
   shutdown();
}

void MockTerminalBus::addAdapter(const std::shared_ptr<Adapter>& adapter)
{
   queue_->bindAdapter(adapter);
   adapter->setQueue(queue_);

   static const auto& adminUser = UserTerminal::create(TerminalUsers::System);
   for (const auto& user : adapter->supportedReceivers()) {
      AdministrativeMessage msg;
      msg.set_component_created(user->value());
      auto env = bs::message::Envelope::makeBroadcast(adminUser, msg.SerializeAsString());
      queue_->pushFill(env);
   }
}

void MockTerminalBus::start()
{
   static const auto& adminUser = UserTerminal::create(TerminalUsers::System);
   AdministrativeMessage msg;
   msg.mutable_start();
   auto env = bs::message::Envelope::makeBroadcast(adminUser, msg.SerializeAsString());
   queue_->pushFill(env);
}

void MockTerminalBus::shutdown()
{
   queue_->terminate();
}

class SettingsMockAdapter : public bs::message::Adapter
{
public:
   SettingsMockAdapter(const std::shared_ptr<spdlog::logger>& logger)
      : logger_(logger)
      , user_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
   {}
   ~SettingsMockAdapter() override = default;

   bool processBroadcast(const bs::message::Envelope& env) override
   {
      if (env.sender->value<TerminalUsers>() == TerminalUsers::Blockchain) {
         ArmoryMessage msg;
         if (!msg.ParseFromString(env.message)) {
            logger_->error("[{}] failed to parse armory msg #{}", __func__, env.foreignId());
            return false;
         }
         if (msg.data_case() == ArmoryMessage::kSettingsRequest) {
            ArmoryMessage msgReply;
            auto msgResp = msgReply.mutable_settings_response();
            ArmorySettings armorySettings;
            armorySettings.name = QLatin1Literal("test");
            armorySettings.netType = NetworkType::TestNet;
            armorySettings.armoryDBIp = QLatin1String("127.0.0.1");
            armorySettings.armoryDBPort = 82;
            msgResp->set_socket_type(armorySettings.socketType);
            msgResp->set_network_type((int)armorySettings.netType);
            msgResp->set_host(armorySettings.armoryDBIp.toStdString());
            msgResp->set_port(std::to_string(armorySettings.armoryDBPort));
            msgResp->set_bip15x_key(armorySettings.armoryDBKey.toStdString());
            msgResp->set_run_locally(armorySettings.runLocally);
            msgResp->set_data_dir(armorySettings.dataDir.toStdString());
            msgResp->set_executable_path(armorySettings.armoryExecutablePath.toStdString());
            msgResp->set_bitcoin_dir(armorySettings.bitcoinBlocksDir.toStdString());
            msgResp->set_db_dir(armorySettings.dbDir.toStdString());
            pushResponse(user_, env, msgReply.SerializeAsString());
            return true;
         }
      }
      return false;
   }

   bool process(const bs::message::Envelope& env) override
   {
      if (env.receiver->value<TerminalUsers>() == TerminalUsers::Settings) {
         SettingsMessage msg;
         if (!msg.ParseFromString(env.message)) {
            logger_->error("[{}] failed to parse settings msg #{}", __func__, env.foreignId());
            return true;
         }
         switch (msg.data_case()) {
         case SettingsMessage::kGetRequest:
            return processGetRequest(env, msg.get_request());
         case SettingsMessage::kPutRequest:        break;
         case SettingsMessage::kArmoryServer:      break;
         case SettingsMessage::kSetArmoryServer:   break;
         case SettingsMessage::kArmoryServersGet:  break;
         case SettingsMessage::kAddArmoryServer:   break;
         case SettingsMessage::kDelArmoryServer:   break;
         case SettingsMessage::kUpdArmoryServer:   break;
         case SettingsMessage::kSignerRequest:     break;
         case SettingsMessage::kSignerSetKey:      break;
         case SettingsMessage::kSignerReset:       break;
         case SettingsMessage::kSignerServersGet:  break;
         case SettingsMessage::kSetSignerServer:   break;
         case SettingsMessage::kAddSignerServer:   break;
         case SettingsMessage::kDelSignerServer:   break;
         case SettingsMessage::kStateGet:          break;
         case SettingsMessage::kReset:             break;
         case SettingsMessage::kResetToState:      break;
         case SettingsMessage::kLoadBootstrap:     break;
         case SettingsMessage::kGetBootstrap:      break;
         case SettingsMessage::kApiPrivkey:        break;
         case SettingsMessage::kApiClientKeys:     break;
         default: break;
         }
      }
      return true;
   }

   bs::message::Adapter::Users supportedReceivers() const override
   {
      return { user_ };
   }

   std::string name() const override { return "Settings"; }

//   std::shared_ptr<OnChainExternalPlug> createOnChainPlug() const;

private:
   bool processGetRequest(const bs::message::Envelope& env
      , const BlockSettle::Terminal::SettingsMessage_SettingsRequest& request)
   {
      SettingsMessage msg;
      auto msgResp = msg.mutable_get_response();
      for (const auto& req : request.requests()) {
         if (req.source() == SettingSource_Local) {
            switch (req.index()) {
            case SetIdx_Initialized: {
               auto resp = msgResp->add_responses();
               resp->set_allocated_request(new SettingRequest(req));
               resp->set_b(true);
            }
               break;
            default: break;
            }
         }
      }
      if (msgResp->responses_size()) {
         return pushResponse(user_, env, msg.SerializeAsString());
      }
      return true;
   }

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
};


class ApiMockAdapter : public bs::message::Adapter
{
public:
   ApiMockAdapter() {}
   ~ApiMockAdapter() override = default;

   bool process(const bs::message::Envelope&) override { return true; }
   bool processBroadcast(const bs::message::Envelope&) override { return false; }

   bs::message::Adapter::Users supportedReceivers() const override {
      return { UserTerminal::create(TerminalUsers::API) };
   }
   std::string name() const override { return "MockAPI"; }
};


MockTerminal::MockTerminal(const std::shared_ptr<spdlog::logger>& logger
   , const std::string &name, const std::shared_ptr<WalletSignerContainer>& signer
   , const std::shared_ptr<TestArmoryConnection>& armory)
   : logger_(logger), name_(name)
{
   bus_ = std::make_shared<MockTerminalBus>(logger, name);
   const auto& userBlockchain = UserTerminal::create(TerminalUsers::Blockchain);
   const auto& userWallets = UserTerminal::create(TerminalUsers::Wallets);
   const auto& signAdapter = std::make_shared<SignerAdapter>(logger, signer);
   bus_->addAdapter(std::make_shared<ApiMockAdapter>());
   bus_->addAdapter(std::make_shared<SettingsMockAdapter>(logger));
   bus_->addAdapter(signAdapter);
   //TODO: add TrackerMockAdapter
   bus_->addAdapter(std::make_shared<WalletsAdapter>(logger_
      , userWallets, signAdapter->createClient(), userBlockchain));
   //bus_->addAdapter(std::make_shared<SettlementAdapter>(logger));
   bus_->addAdapter(std::make_shared<BlockchainAdapter>(logger, userBlockchain
      , armory));
}

void MockTerminal::start()
{
   bus_->start();
}

void MockTerminal::stop()
{
   bus_->shutdown();
}
