#ifndef __TEST_CC_H__
#define __TEST_CC_H__

#include <atomic>
#include <memory>
#include <set>
#include <gtest/gtest.h>
#include <QObject>
#include <QMutex>
#include "Address.h"
#include "BlockchainMonitor.h"


namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
   namespace core {
      class Wallet;
   }
}

class TestCC : public QObject, public ::testing::Test
{
   Q_OBJECT

protected:
   explicit TestCC();

   void SetUp() override;
   void TearDown() override;

protected:
   const double   initialAmount_ = 1.01;
   const uint32_t ccFundingAmount_ = 1000;
   const uint64_t ccLotSize_ = 526;
   std::shared_ptr<bs::core::Wallet>   ccSignWallet_;
   std::shared_ptr<bs::sync::Wallet>   ccWallet_;
   std::shared_ptr<bs::core::Wallet>   xbtSignWallet_;
   std::shared_ptr<bs::sync::Wallet>   xbtWallet_;
   std::shared_ptr<bs::sync::WalletsManager> syncMgr_;
   bs::Address    genesisAddr_;
   bs::Address    fundingAddr_;
   bs::Address    recvAddr_;
};


#endif // __TEST_CC_H__
