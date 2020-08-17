/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef BS_SERVER_ADAPTER_H
#define BS_SERVER_ADAPTER_H

#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
class ConnectionManager;

class BsServerAdapter : public bs::message::Adapter
{
public:
   BsServerAdapter(const std::shared_ptr<spdlog::logger> &);
   ~BsServerAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "BS Servers"; }

private:

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<ConnectionManager>  connMgr_;
};


#endif	// BS_SERVER_ADAPTER_H
