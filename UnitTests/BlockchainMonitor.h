#ifndef __BLOCKCHAIN_MONITOR_H__
#define __BLOCKCHAIN_MONITOR_H__

#include <atomic>
#include <memory>
#include <vector>
#include <QObject>
#include "ArmoryObject.h"
#include "BinaryData.h"
#include "ClientClasses.h"
#include "ThreadSafeClasses.h"


namespace bs {
   namespace sync {
      class Wallet;
   }
}
class ArmoryConnection;

class BlockchainMonitor : public QObject
{
   Q_OBJECT

public:
   BlockchainMonitor(const std::shared_ptr<ArmoryObject> &);

   uint32_t waitForNewBlocks(uint32_t targetHeight = 0);
   std::vector<bs::TXEntry> waitForZC(void);
   bool waitForWalletReady(
      const std::vector<std::string>&, double timeoutInSec = 30);

   static bool waitForFlag(std::atomic_bool &, double timeoutInSec = 30);

private:
   std::shared_ptr<ArmoryObject>   armory_;
   std::atomic_bool  receivedNewBlock_;
   Queue<std::vector<bs::TXEntry>> zcQueue_;
   Queue<std::vector<BinaryData>> refreshQueue_;
};

#endif // __BLOCKCHAIN_MONITOR_H__
