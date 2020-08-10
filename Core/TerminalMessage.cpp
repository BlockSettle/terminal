/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TerminalMessage.h"
#include "Message/Adapter.h"

using namespace bs::message;

static const std::map<int, std::string> kTerminalUsersMapping = {
   { static_cast<int>(TerminalUsers::BROADCAST), "Broadcast" },
   { static_cast<int>(TerminalUsers::Signer), "Signer" },
   { static_cast<int>(TerminalUsers::API), "API" },
   { static_cast<int>(TerminalUsers::Settings), "Settings" },
   { static_cast<int>(TerminalUsers::BsServer), "BsServer" },
   { static_cast<int>(TerminalUsers::Matching), "Matching" },
   { static_cast<int>(TerminalUsers::Assets), "Assets" },
   { static_cast<int>(TerminalUsers::MktData), "MarketData" },
   { static_cast<int>(TerminalUsers::MDHistory), "MDHistory" },
   { static_cast<int>(TerminalUsers::Blockchain), "Armory" },
   { static_cast<int>(TerminalUsers::Wallets), "Wallets" },
   { static_cast<int>(TerminalUsers::Settlement), "Settlement" },
   { static_cast<int>(TerminalUsers::Chat), "Chat" }
};

std::string UserTerminal::name() const
{
   const auto itAcc = kTerminalUsersMapping.find(value());
   return (itAcc == kTerminalUsersMapping.end())
      ? User::name() : itAcc->second;
}

/*namespace bs {
   namespace message {
      Envelope pbEnvelope(uint64_t id, UsersPB sender, UsersPB receiver
         , const TimeStamp &posted, const TimeStamp &execAt, const std::string &message
         , bool request)
      {
         return Envelope{ id, std::make_shared<UserPB>(sender)
            , std::make_shared<UserPB>(receiver), posted, execAt, message, request };
      }

      Envelope pbEnvelope(uint64_t id, UsersPB sender, const std::shared_ptr<User> &receiver
         , const std::string &message)
      {
         return Envelope{ id, std::make_shared<UserPB>(sender), receiver
            , {}, {}, message, false };
      }

      Envelope pbEnvelope(UsersPB sender, UsersPB receiver, const std::string &message
         , bool request, const TimeStamp &execAt)
      {
         return Envelope{ 0, std::make_shared<UserPB>(sender), std::make_shared<UserPB>(receiver)
            , {}, execAt, message, request };
      }

      Envelope pbEnvelope(const std::shared_ptr<User> &sender, UsersPB receiver
         , const std::string &message, bool request, const TimeStamp &execAt)
      {
         return Envelope{ 0, sender, std::make_shared<UserPB>(receiver)
            , {}, execAt, message, request };
      }
   }  //namespace message
}  //namespace bs
*/

TerminalInprocBus::TerminalInprocBus(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{  // we can create multiple queues if needed and distribute them on adapters
   queue_ = std::make_shared<Queue>(std::make_shared<Router>(logger), logger
      , kTerminalUsersMapping);
}

TerminalInprocBus::~TerminalInprocBus()
{
   shutdown();
}

void TerminalInprocBus::addAdapter(const std::shared_ptr<Adapter> &adapter)
{
   queue_->bindAdapter(adapter);
   adapter->setQueue(queue_);
   auto runner = std::dynamic_pointer_cast<bs::MainLoopRuner>(adapter);
   if (runner) {
      runnableAdapter_ = runner;
   }
}

void TerminalInprocBus::shutdown()
{
   runnableAdapter_.reset();
   queue_->terminate();
}

bool TerminalInprocBus::run()
{
   if (!runnableAdapter_) {
      return false;
   }
   runnableAdapter_->run();
   return true;
}
