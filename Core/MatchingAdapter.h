/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MATCHING_ADAPTER_H
#define MATCHING_ADAPTER_H

#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}

class MatchingAdapter : public bs::message::Adapter
{
public:
   MatchingAdapter(const std::shared_ptr<spdlog::logger> &);
   ~MatchingAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Matching"; }

private:

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
};


#endif	// MATCHING_ADAPTER_H
