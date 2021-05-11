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
   { static_cast<int>(TerminalUsers::System), "System  " },
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
      bs::message::Envelope env{ adminUser, {}, msg.SerializeAsString() };
      queue_->pushFill(env);
   }
}

void TerminalInprocBus::start()
{
   static const auto &adminUser = UserTerminal::create(TerminalUsers::System);
   AdministrativeMessage msg;
   msg.mutable_start();
   bs::message::Envelope env{ adminUser, {}, msg.SerializeAsString() };
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
