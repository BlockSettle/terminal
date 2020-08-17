/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MDHIST_ADAPTER_H
#define MDHIST_ADAPTER_H

#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}

class MDHistAdapter : public bs::message::Adapter
{
public:
   MDHistAdapter(const std::shared_ptr<spdlog::logger> &);
   ~MDHistAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "MDHistory"; }

private:

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
};


#endif	// MDHIST_ADAPTER_H
