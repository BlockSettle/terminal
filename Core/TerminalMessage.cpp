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

#include "common.pb.h"

using namespace bs::message;
using namespace BlockSettle::Common;

static const std::map<int, std::string> kTerminalUsersMapping = {
   { static_cast<int>(TerminalUsers::BROADCAST), "Broadcast" },
   { static_cast<int>(TerminalUsers::System), "System" },
   { static_cast<int>(TerminalUsers::Signer), "Signer" },
   { static_cast<int>(TerminalUsers::API), "API" },
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
   { static_cast<int>(TerminalUsers::Chat), "Chat" }
};
static const std::string kMainQueue = "Main";

std::string UserTerminal::name() const
{
   const auto itAcc = kTerminalUsersMapping.find(value());
   return (itAcc == kTerminalUsersMapping.end())
      ? User::name() : itAcc->second;
}


TerminalInprocBus::TerminalInprocBus(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{
   queues_[kMainQueue] = std::make_shared<Queue>(std::make_shared<Router>(logger)
      , logger, kMainQueue);
}

TerminalInprocBus::~TerminalInprocBus()
{
   shutdown();
}

void TerminalInprocBus::addAdapter(const std::shared_ptr<Adapter> &adapter)
{
   const auto& queue = queues_.at(kMainQueue);
   queue->bindAdapter(adapter);
   adapter->setQueue(queue);
   const auto &runner = std::dynamic_pointer_cast<bs::MainLoopRuner>(adapter);
   if (runner) {
      logger_->info("[{}] set runnable adapter {}", __func__, adapter->name());
      runnableAdapter_ = runner;
   }

   const auto& relay = std::dynamic_pointer_cast<bs::message::RelayAdapter>(adapter);
   if (relay) {
      logger_->info("[{}] set relay adapter {}", __func__, adapter->name());
      relayAdapter_ = relay;
   }

   sendLoading(adapter, queue);
}

void bs::message::TerminalInprocBus::addAdapterWithQueue(const std::shared_ptr<Adapter>& adapter
   , const std::string& qName)
{
   if (qName == kMainQueue) {
      throw std::runtime_error("main queue name reused");
   }
   std::shared_ptr<bs::message::Queue> queue;
   const auto& itQueue = queues_.find(qName);
   if (itQueue == queues_.end()) {
      queue = std::make_shared<Queue>(std::make_shared<Router>(logger_)
         , logger_, qName);
      queues_[qName] = queue;
   }
   else {
      queue = itQueue->second;
   }

   queue->bindAdapter(adapter);
   adapter->setQueue(queue);

   if (relayAdapter_) {
      queue->bindAdapter(relayAdapter_);
      relayAdapter_->setQueue(queue);
      logger_->debug("[{}] relay {} bound to {}", __func__, relayAdapter_->name(), qName);
   }
   else {
      logger_->warn("[{}] no relay adapter attached to {}", __func__, qName);
   }
   sendLoading(adapter, queues_.at(kMainQueue));
}

void TerminalInprocBus::start()
{
   logger_->debug("[TerminalInprocBus::start]");
   static const auto &adminUser = UserTerminal::create(TerminalUsers::System);
   AdministrativeMessage msg;
   msg.mutable_start();
   auto env = bs::message::Envelope::makeBroadcast(adminUser, msg.SerializeAsString());
   queues_.at(kMainQueue)->pushFill(env);
}

void bs::message::TerminalInprocBus::sendLoading(const std::shared_ptr<Adapter>& adapter
   , const std::shared_ptr<Queue>& queue)
{
   static const auto& adminUser = UserTerminal::create(TerminalUsers::System);
   for (const auto& user : adapter->supportedReceivers()) {
      AdministrativeMessage msg;
      msg.set_component_created(user->value());
      auto env = bs::message::Envelope::makeBroadcast(adminUser, msg.SerializeAsString());
      queue->pushFill(env);
   }
}

void TerminalInprocBus::shutdown()
{
   runnableAdapter_.reset();
   relayAdapter_.reset();
   for (const auto& q : queues_) {
      q.second->terminate();
   }
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
