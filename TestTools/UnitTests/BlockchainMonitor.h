#ifndef __BLOCKCHAIN_MONITOR_H__
#define __BLOCKCHAIN_MONITOR_H__

#include <atomic>
#include <memory>
#include <vector>
#include <QObject>
#include "BinaryData.h"

using namespace std;
#include "LedgerEntryData.h"


namespace bs {
   class Wallet;
}

class BlockchainMonitor : public QObject
{
   Q_OBJECT

public:
   explicit BlockchainMonitor();

   uint32_t waitForNewBlocks(uint32_t targetHeight = 0);
   bool waitForZC(double timeoutInSec = 30) { return waitForFlag(receivedZC_, timeoutInSec); }
   std::vector<LedgerEntryData> getZCentries() const { return zcEntries_; }

   static bool waitForFlag(std::atomic_bool &, double timeoutInSec = 30);
   static bool waitForWalletReady(const std::shared_ptr<bs::Wallet> &, double timeoutInSec = 30);

private:
   std::atomic_bool  receivedNewBlock_;
   std::atomic_bool  receivedZC_;
   std::vector<LedgerEntryData>  zcEntries_;
};

#endif // __BLOCKCHAIN_MONITOR_H__
