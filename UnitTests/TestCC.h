/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   class CCResolver : public bs::sync::CCDataResolver
   {
   public:
      CCResolver(uint64_t lotSize, const bs::Address &genAddr)
         : lotSize_(lotSize), genAddr_(genAddr) {}
      std::string nameByWalletIndex(const bs::hd::Path::Elem) const override { return "BLK"; }
      uint64_t lotSizeFor(const std::string &cc) const override { return lotSize_; }
      bs::Address genesisAddrFor(const std::string &cc) const override { return genAddr_; }
      std::string descriptionFor(const std::string &cc) const override { return {}; }
      std::vector<std::string> securities() const override { return {"BLK"}; }
   private:
      const uint64_t    lotSize_;
      const bs::Address genAddr_;
   };
   std::shared_ptr<CCResolver> resolver_;

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
      READHEX("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
   BinaryData coinbasePubKey_;
   BinaryData coinbaseScrAddr_;
   std::shared_ptr<ResolverOneAddress> coinbaseFeed_;
   
   std::map<unsigned, BinaryData> coinbaseHashes_;
   unsigned coinbaseCounter_ = 0;

   SecureBinaryData passphrase_;
};

#endif // __TEST_CC_H__
