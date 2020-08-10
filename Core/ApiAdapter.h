/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
            else {
               return "User#" + std::to_string(value());
            }
         }
      };
   }
}
class ApiBus;


class ApiBusAdapter : public bs::message::Adapter
{
   friend class ApiBus;
private:
   void setUserId(const bs::message::UserValue);
};


class ApiAdapter : public bs::message::Adapter, public bs::MainLoopRuner
{
//   friend class GenoaBusGateway;
public:
   ApiAdapter(const std::shared_ptr<spdlog::logger> &);
   ~ApiAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "API"; }

   void add(const std::shared_ptr<ApiBusAdapter> &);
   void run() override;

private:
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::Bus>   apiBus_;
//   std::shared_ptr<GenoaBusGateway>    gwAdapter_;
};


#endif	// API_ADAPTER_H
