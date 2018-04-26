#include "BlockchainMonitor.h"
#include <QApplication>
#include <QDateTime>
#include <QThread>
#include "MetaData.h"
#include "PyBlockDataManager.h"


BlockchainMonitor::BlockchainMonitor() : QObject(nullptr), receivedNewBlock_(false), receivedZC_(false)
{
   connect(PyBlockDataManager::instance().get(), &PyBlockDataManager::newBlock, [this] { receivedNewBlock_ = true; });
   connect(PyBlockDataManager::instance().get(), &PyBlockDataManager::zeroConfReceived
      , [this](const std::vector<LedgerEntryData> &entries) {
      zcEntries_ = entries;
      receivedZC_ = true;
   });
}

uint32_t BlockchainMonitor::waitForNewBlocks(uint32_t targetHeight)
{
   while (!receivedNewBlock_ || (targetHeight && (PyBlockDataManager::instance()->GetTopBlockHeight() < targetHeight))) {
      QApplication::processEvents();
      QThread::msleep(1);
   }
   receivedNewBlock_ = false;
   return PyBlockDataManager::instance()->GetTopBlockHeight();
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

bool BlockchainMonitor::waitForWalletReady(const std::shared_ptr<bs::Wallet> &wallet, double timeoutInSec)
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
