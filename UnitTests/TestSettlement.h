#ifndef __TEST_SETTLEMENT_H__
#define __TEST_SETTLEMENT_H__

#include <atomic>
#include <memory>
#include <set>
#include <gtest/gtest.h>
#include <QObject>
#include <QMutex>
#include "Address.h"
#include "BlockchainMonitor.h"
#include "SettlementMonitor.h"
#include "TestEnv.h"
#include "TradesVerification.h"


namespace bs {
   namespace core {
      class Wallet;
      namespace hd {
         class Wallet;
         class Leaf;
      }
      class WalletsManager;
   }
   namespace sync {
      class Wallet;
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class WalletsManager;
   }
}

class TestSettlement : public ::testing::Test
{
protected:
   TestSettlement();
   ~TestSettlement() override;

   void SetUp() override;
   void TearDown() override;

   bool waitForPayIn() { return BlockchainMonitor::waitForFlag(receivedPayIn_); }
   bool waitForPayOut() { return BlockchainMonitor::waitForFlag(receivedPayOut_); }
//   bool waitForSettlWallet() { return BlockchainMonitor::waitForFlag(settlWalletReady_); }
   
   void mineBlocks(unsigned count);
   void sendTo(uint64_t value, bs::Address& addr);

public:
   std::shared_ptr<TestEnv> envPtr_;
   SecureBinaryData passphrase_;

protected:
   const size_t   nbParties_ = 2;
   const double   initialTransferAmount_ = 1.23;
   std::vector<std::shared_ptr<bs::core::hd::Wallet>> hdWallet_;
   std::vector<std::shared_ptr<bs::core::hd::Leaf>>   authWallet_;
   std::shared_ptr<bs::core::WalletsManager>          walletsMgr_;
   std::shared_ptr<bs::sync::WalletsManager>          syncMgr_;
   std::vector<std::shared_ptr<bs::core::Wallet>>     xbtWallet_;
   std::vector<bs::Address>      authAddrs_;
   std::vector<SecureBinaryData> authKeys_;
   std::vector<bs::Address>      fundAddrs_;
   SecureBinaryData              settlementId_;
   std::vector<SecureBinaryData> userId_;
   std::map<bs::Address, std::shared_ptr<bs::core::hd::Leaf>>  settlLeafMap_;
   std::atomic_bool  receivedPayIn_{ false };
   std::atomic_bool  receivedPayOut_{ false };
   bs::PayoutSignatureType  poType_ = bs::PayoutSignatureType::Undefined;

private:
   QMutex            mtxWalletId_;
   std::set<QString> walletsReady_;

   SecureBinaryData coinbasePrivKey_ =
      READHEX("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
   BinaryData coinbasePubKey_;
   BinaryData coinbaseScrAddr_;
   std::shared_ptr<ResolverOneAddress> coinbaseFeed_;

   std::map<unsigned, BinaryData> coinbaseHashes_;
   unsigned coinbaseCounter_ = 0;

   std::unique_ptr<SingleUTWalletACT>  act_;

private:
   void onWalletReady(const QString &id);
};


#endif // __TEST_SETTLEMENT_H__
