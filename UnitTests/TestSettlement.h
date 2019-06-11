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

class TestSettlement : public QObject, public ::testing::Test
{
   Q_OBJECT

protected:
   TestSettlement();

   void SetUp() override;
   void TearDown() override;

   bool waitForPayIn(double timeoutInSec = 30) { return BlockchainMonitor::waitForFlag(receivedPayIn_, timeoutInSec); }
   bool waitForPayOut(double timeoutInSec = 30) { return BlockchainMonitor::waitForFlag(receivedPayOut_, timeoutInSec); }
   bool waitForSettlWallet(double timeoutInSec = 10) { return BlockchainMonitor::waitForFlag(settlWalletReady_, timeoutInSec); }
   
   void mineBlocks(unsigned count);
   void sendTo(uint64_t value, bs::Address& addr);

protected:
   const size_t   nbParties_ = 2;
   const double   initialTransferAmount_ = 1.23;
   std::vector<std::shared_ptr<bs::sync::hd::Wallet>> hdWallet_;
   std::vector<std::shared_ptr<bs::sync::hd::Leaf>>   authWallet_;
   std::shared_ptr<bs::core::WalletsManager>          walletsMgr_;
   std::shared_ptr<bs::sync::WalletsManager>          syncMgr_;
   std::vector<std::shared_ptr<bs::core::Wallet>>     signWallet_;
   std::vector<std::shared_ptr<bs::core::hd::Leaf>>   authSignWallet_;
   std::vector<bs::Address>                     authAddr_;
   std::vector<bs::Address>                     fundAddr_;
   SecureBinaryData              settlementId_;
   std::vector<SecureBinaryData> userId_;
   std::atomic_bool  receivedPayIn_, receivedPayOut_;
   bs::PayoutSigner::Type  poType_ = bs::PayoutSigner::Type::SignatureUndefined;

private:
   std::atomic_bool  settlWalletReady_;
   QMutex            mtxWalletId_;
   std::set<QString> walletsReady_;
   std::shared_ptr<TestEnv> envPtr_;

   SecureBinaryData coinbasePrivKey_ =
      READHEX("0102030405060708090A0B0C0D0E0F1112131415161718191A1B1C1D1E1F");
   BinaryData coinbasePubKey_;
   BinaryData coinbaseScrAddr_;
   std::shared_ptr<ResolverOneAddress> coinbaseFeed_;

   std::map<unsigned, BinaryData> coinbaseHashes_;
   unsigned coinbaseCounter_ = 0;

   SecureBinaryData passphrase_;

private:
   void onWalletReady(const QString &id);
};


#endif // __TEST_SETTLEMENT_H__
