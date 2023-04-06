/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef API_ADAPTER_H
#define API_ADAPTER_H

#include "Message/Adapter.h"
#include "TerminalMessage.h"

namespace bs {
   namespace message {
      class UserAPI : public User
      {
      public:
         UserAPI(UserValue value) : User(value) {}

         bool isFallback() const override
         {
            return (value() == 0);
         }

         bool isBroadcast() const override
         {
            return (value() < 0);
         }

         std::string name() const override
         {
            if (value() == 0) {
               return "Gateway";
            }
            else if (value() < 0) {
               return "Broadcast";
            }
            else if (!name_.empty()) {
               return name_;
            }
            else {
               return "User#" + std::to_string(value());
            }
         }

         void setName(const std::string& name)
         {
            name_ = name;
         }

      private:
         std::string name_;
      };
   }
}
class ApiBus;
class ApiBusGateway;


class ApiBusAdapter : public bs::message::Adapter
{
public:
   Users supportedReceivers() const override { return { user_ }; }

   void setUserId(const bs::message::UserValue userVal)
   {
      user_ = std::make_shared<bs::message::UserAPI>(userVal);
      user_->setName(name());
   }

protected:
   bool pushFill(bs::message::Envelope&) override;

protected:
   std::shared_ptr<bs::message::UserAPI>  user_;
};


class ApiAdapter : public bs::message::RelayAdapter, public bs::MainLoopRuner
{
   friend class ApiBusGateway;
public:
   ApiAdapter(const std::shared_ptr<spdlog::logger> &);
   ~ApiAdapter() override = default;

   bs::message::ProcessingResult process(const bs::message::Envelope&) override;
   bool processBroadcast(const bs::message::Envelope&) override;
   bool processTimeout(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_, fallbackUser_ }; }
   std::string name() const override { return "API"; }

   void add(const std::shared_ptr<ApiBusAdapter> &);
   void run(int &argc, char **argv) override;

private:
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<ApiBus>             apiBus_;
   std::shared_ptr<bs::MainLoopRuner>  mainLoopAdapter_;
   std::shared_ptr<ApiBusGateway>      gwAdapter_;

   bs::message::UserValue  nextApiUser_{ 0 };
};

#endif	// API_ADAPTER_H
