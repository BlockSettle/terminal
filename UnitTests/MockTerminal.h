/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __MOCK_TERMINAL_H__
#define __MOCK_TERMINAL_H__

#include <memory>
#include <string>
#include "Message/Bus.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
   }
}
class MockTerminal;
class TestArmoryConnection;
class WalletSignerContainer;

class MockTerminalBus : public bs::message::Bus
{
   friend class MockTerminal;
public:
   MockTerminalBus(const std::shared_ptr<spdlog::logger>&
      , const std::string &name);
   ~MockTerminalBus() override;

   void addAdapter(const std::shared_ptr<bs::message::Adapter>&) override;

private:
   void start();
   void shutdown();

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::Queue> queue_;
};

class MockTerminal
{
public:
   MockTerminal(const std::shared_ptr<spdlog::logger>& logger
      , const std::string &name, const std::shared_ptr<WalletSignerContainer> &
      , const std::shared_ptr<TestArmoryConnection> &);

   std::shared_ptr<MockTerminalBus> bus() const { return bus_; }
   std::string name() const { return name_; }

   void start();
   void stop();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<MockTerminalBus> bus_;
   std::string name_;
};

#endif // __MOCK_TERMINAL_H__
