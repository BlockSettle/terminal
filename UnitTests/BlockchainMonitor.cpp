/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BlockchainMonitor.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWallet.h"


BlockchainMonitor::BlockchainMonitor(const std::shared_ptr<ArmoryConnection> &armory)
   : ArmoryCallbackTarget()
{
   init(armory.get());
}

BlockchainMonitor::~BlockchainMonitor()
{
   cleanup();
}

uint32_t BlockchainMonitor::waitForNewBlocks(uint32_t targetHeight)
{
   while (!receivedNewBlock_) {
      std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
   }
   receivedNewBlock_ = false;
   return armory_->topBlock();
}

bool BlockchainMonitor::waitForFlag(std::atomic_bool &flag, const std::chrono::milliseconds timeout)
{
   using namespace std::chrono_literals;
   const auto napTime = 10ms;
   for (auto elapsed = 0ms; elapsed < timeout; elapsed += napTime) {
      if (flag) {
         return true;
      }
      std::this_thread::sleep_for(napTime);
   }
   return false;
}

std::vector<bs::TXEntry> BlockchainMonitor::waitForZC()
{
   while (true) {
      try {
         auto zcVec = zcQueue_.pop_front();
         return zcVec;
      }
      catch (Armory::Threading::IsEmpty&)
      {}

      std::this_thread::sleep_for(std::chrono::milliseconds{10});
   }
}

std::vector<bs::TXEntry> BlockchainMonitor::waitForZCs(int count)
{
   std::vector<bs::TXEntry> result;
   while (int(result.size()) < count) {
      auto newItems = waitForZC();
      result.insert(result.end(), std::make_move_iterator(newItems.begin()), std::make_move_iterator(newItems.end()));
   }
   return result;
}
