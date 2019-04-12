#include "BlockchainMonitor.h"
#include <QApplication>
#include <QDateTime>
#include <QThread>
#include "Wallets/SyncWallet.h"


BlockchainMonitor::BlockchainMonitor(const std::shared_ptr<ArmoryObject> &armory)
   : QObject(nullptr), armory_(armory), receivedNewBlock_(false)
{
   connect(armory_.get(), &ArmoryObject::newBlock, [this](unsigned int) { receivedNewBlock_ = true; });
   connect(armory_.get(), &ArmoryObject::zeroConfReceived
      , [this](std::vector<bs::TXEntry> entries) {
      zcQueue_.push_back(std::move(entries));
   });
   connect(armory_.get(), &ArmoryObject::refresh,
      [this](std::vector<BinaryData> ids, bool)
      { refreshQueue_.push_back(move(ids)); }
   );
}

uint32_t BlockchainMonitor::waitForNewBlocks(uint32_t targetHeight)
{
   while (!receivedNewBlock_ || (targetHeight && (armory_->topBlock() < targetHeight))) {
      QApplication::processEvents();
      QThread::msleep(50);
   }
   receivedNewBlock_ = false;
   return armory_->topBlock();
}

bool BlockchainMonitor::waitForFlag(std::atomic_bool &flag, double timeoutInSec)
{
   const auto curTime = QDateTime::currentDateTime();
   while (!flag) {
      QApplication::processEvents();
      if (curTime.msecsTo(QDateTime::currentDateTime()) > (timeoutInSec * 1000)) {
         return false;
      }
      QThread::msleep(50);
   }
   flag = false;
   return true;
}

bool BlockchainMonitor::waitForWalletReady(const std::vector<std::string>& ids, double timeoutInSec)
{
   if (ids.size() == 0)
      return false;

   std::set<BinaryData> idSet;
   for (auto& id : ids)
   {
      BinaryData idBd((const uint8_t*)id.c_str(), id.size());
      idSet.insert(idBd);
   }

   const auto curTime = QDateTime::currentDateTime();
   while (true) 
   {
      QApplication::processEvents();

      try
      {
         auto&& idVec = refreshQueue_.pop_front();
         for (auto& id : idVec)
         {
            auto iter = idSet.find(id);
            if (iter != idSet.end())
               idSet.erase(iter);
         }
      }
      catch (IsEmpty&)
      {}

      if (idSet.size() == 0)
         return true;

      if (curTime.msecsTo(QDateTime::currentDateTime()) > (timeoutInSec * 100000000)) {
         return false;
      }
      
      QThread::msleep(100);
   }
   return true;
}

std::vector<bs::TXEntry> BlockchainMonitor::waitForZC()
{
   while (true)
   {
      QApplication::processEvents();

      try
      {
         auto&& zcVec = zcQueue_.pop_front();
         return zcVec;
      }
      catch (IsEmpty&)
      {}

      QThread::msleep(10);
   }
}

