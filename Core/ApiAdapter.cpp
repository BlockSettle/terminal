/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ApiAdapter.h"
#include <spdlog/spdlog.h>


class ApiRouter : public bs::message::Router
{
public:
   ApiRouter(const std::shared_ptr<spdlog::logger> &logger)
      : bs::message::Router(logger)
   {}

protected:
   bool isDefaultRouted(const bs::message::Envelope &env) const override
   {
      if (std::dynamic_pointer_cast<bs::message::UserTerminal>(env.receiver)) {
         return true;
      }
      return bs::message::Router::isDefaultRouted(env);
   }
};


class ApiBus : public bs::message::Bus
{
public:
   ApiBus(const std::shared_ptr<spdlog::logger> &logger)
   {
      queue_ = std::make_shared<bs::message::Queue>(
         std::make_shared<ApiRouter>(logger), logger, "API");
   }

   ~ApiBus() override
   {
      queue_->terminate();
   }

   void addAdapter(const std::shared_ptr<bs::message::Adapter> &adapter) override
   {
      queue_->bindAdapter(adapter);
      adapter->setQueue(queue_);
      adapters_.push_back(adapter);
   }

private:
   std::shared_ptr<bs::message::QueueInterface>       queue_;
   std::vector<std::shared_ptr<bs::message::Adapter>> adapters_;
};


class ApiBusGateway : public ApiBusAdapter
{
public:
   ApiBusGateway(const std::shared_ptr<spdlog::logger> &logger
      , ApiAdapter *parent)
      : logger_(logger), parent_(parent)
      , user_(std::make_shared<bs::message::UserAPI>(0))
      , userTermBroadcast_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::BROADCAST))
   {}

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override
   {
      return { user_ };
   }
   std::string name() const override { return "APIbusGW"; }

   bool process(const bs::message::Envelope &env) override
   {
      if (!env.receiver) {
         return true;   // ignore unsolicited broadcasts
      }
      auto envCopy = env;
      envCopy.id = 0;
      envCopy.sender = parent_->user_;

      if (std::dynamic_pointer_cast<bs::message::UserTerminal>(env.receiver)) {
         if (parent_->pushFill(envCopy)) {
            if (env.request) {
               idMap_[envCopy.id] = { env.id, env.sender };
            }
            return true;
         }
         else {
            return false;
         }
      }
      else if (env.receiver->isFallback()) {
         if (env.request) {
            envCopy.receiver = userTermBroadcast_;
            return parent_->pushFill(envCopy);
         }
         else {
            const auto &itId = idMap_.find(env.id);
            if (itId == idMap_.end()) {
               envCopy.receiver = userTermBroadcast_;
               return parent_->pushFill(envCopy);
            }
            else {
               envCopy.id = itId->second.id;
               envCopy.receiver = itId->second.requester;
               if (parent_->push(envCopy)) {
                  idMap_.erase(env.id);
               }
               return true;
            }
         }
      }
      return true;
   }

   bool pushToApiBus(const bs::message::Envelope &env)
   {
      auto envCopy = env;
      envCopy.id = 0;
      envCopy.receiver.reset();
      if (!env.request && env.receiver) {
         const auto& itIdMap = idMap_.find(env.id);
         if (itIdMap != idMap_.end()) {
            envCopy.id = itIdMap->second.id;
            envCopy.receiver = itIdMap->second.requester;
            if (push(envCopy)) {
               idMap_.erase(itIdMap);
               return true;
            }
            return false;
         }
      }
      bool rc = pushFill(envCopy);
      if (rc && env.request) {
         idMap_[envCopy.id] = { env.id, env.sender };
      }
      return rc;
   }

private:
   std::shared_ptr<spdlog::logger>  logger_;
   ApiAdapter  * parent_{ nullptr };
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<bs::message::User>  userTermBroadcast_;

   struct RequestData {
      uint64_t id;
      std::shared_ptr<bs::message::User>  requester;
   };
   std::map<uint64_t, RequestData>  idMap_;
};


ApiAdapter::ApiAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::API))
{
   apiBus_ = std::make_shared<ApiBus>(logger);
   gwAdapter_ = std::make_shared<ApiBusGateway>(logger, this);
   apiBus_->addAdapter(gwAdapter_);
}

void ApiAdapter::run(int &argc, char **argv)
{
   if (mainLoopAdapter_) {
      mainLoopAdapter_->run(argc, argv);
   }
   else {
      while (true) {
         std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
      }
   }
}

void ApiAdapter::add(const std::shared_ptr<ApiBusAdapter> &adapter)
{
   const auto &runner = std::dynamic_pointer_cast<bs::MainLoopRuner>(adapter);
   if (runner) {
      if (mainLoopAdapter_) {
         logger_->error("[{}] main loop adapter already set - ignoring {}"
            , __func__, adapter->name());
      }
      else {
         mainLoopAdapter_ = runner;
      }
   }
   const auto userId = ++nextApiUser_;
   logger_->debug("[{}] {} has id {}", __func__, adapter->name(), userId);
   adapter->setUserId(userId);
   apiBus_->addAdapter(adapter);
}

bool ApiAdapter::process(const bs::message::Envelope &env)
{
   return gwAdapter_->pushToApiBus(env);
}
