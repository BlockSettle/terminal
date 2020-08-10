/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ApiAdapter.h"
#include <spdlog/spdlog.h>


/*class ApiBus : public bs::message::Bus
{
public:
   ApiBus(const std::shared_ptr<spdlog::logger> &);
   ~ApiBus() override;

   void addAdapter(const std::shared_ptr<bs::message::Adapter> &) override;

   void SetCommunicationDumpEnabled(bool dumpCommunication) override;

private:
   class ClientAdapter;
   class ServerAdapter;

   std::shared_ptr<bs::message::QueueInterface> queue_;
   const std::vector<std::shared_ptr<ApiBusAdapter>>  adapters_;
};*/


ApiAdapter::ApiAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{}

void ApiAdapter::run()
{
   logger_->debug("[{}]", __func__);
}

void ApiAdapter::add(const std::shared_ptr<ApiBusAdapter> &)
{

}

bool ApiAdapter::process(const bs::message::Envelope &)
{
   logger_->debug("[{}]", __func__);
   return true;
}
