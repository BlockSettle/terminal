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
#include "TestEnv.h"

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

   void mineBlocks(unsigned);
   void sendTo(uint64_t, bs::Address&);

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
   bs::Address    recvAddr_;
   std::shared_ptr<TestEnv> envPtr_;

   SecureBinaryData coinbasePrivKey_ =
      READHEX("0102030405060708090A0B0C0D0E0F1112131415161718191A1B1C1D1E1F");
   BinaryData coinbasePubKey_;
   BinaryData coinbaseScrAddr_;
   std::shared_ptr<ResolverCoinbase> coinbaseFeed_;
   
   std::map<unsigned, BinaryData> coinbaseHashes_;
   unsigned coinbaseCounter_ = 0;

   SecureBinaryData passphrase_;
};


#endif // __TEST_CC_H__
