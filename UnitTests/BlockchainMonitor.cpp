#include "BlockchainMonitor.h"
#include <QApplication>
#include <QDateTime>
#include <QThread>
#include "Wallets/SyncWallet.h"


BlockchainMonitor::BlockchainMonitor(const std::shared_ptr<ArmoryObject> &armory)
   : QObject(nullptr), armory_(armory), receivedNewBlock_(false), receivedZC_(false)
{
   connect(armory_.get(), &ArmoryObject::newBlock, [this](unsigned int) { receivedNewBlock_ = true; });
   connect(armory_.get(), &ArmoryObject::zeroConfReceived
      , [this](const std::vector<bs::TXEntry> entries) {
      zcEntries_ = entries;
      receivedZC_ = true;
   });
}

uint32_t BlockchainMonitor::waitForNewBlocks(uint32_t targetHeight)
{
   while (!receivedNewBlock_ || (targetHeight && (armory_->topBlock() < targetHeight))) {
      QApplication::processEvents();
      QThread::msleep(1);
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
      QThread::msleep(1);
   }
   flag = false;
   return true;
}

bool BlockchainMonitor::waitForWalletReady(const std::shared_ptr<bs::sync::Wallet> &wallet, double timeoutInSec)
{
   const auto curTime = QDateTime::currentDateTime();
   while (!wallet->isBalanceAvailable()) {
      QApplication::processEvents();
      if (curTime.msecsTo(QDateTime::currentDateTime()) > (timeoutInSec * 1000)) {
         return false;
      }
      QThread::msleep(1);
   }
   return true;
}
