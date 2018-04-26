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
   namespace hd {
      class Leaf;
      class Wallet;
   }
   class Wallet;
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
   std::shared_ptr<bs::hd::Leaf>    ccWallet_;
   std::shared_ptr<bs::Wallet>      xbtWallet_;
   bs::Address    genesisAddr_;
   bs::Address    fundingAddr_;
   bs::Address    recvAddr_;
};


#endif // __TEST_CC_H__
