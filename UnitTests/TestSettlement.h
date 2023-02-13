/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
#include "Wallets/SignContainer.h"
#include "TestEnv.h"


namespace bs {
   enum class PayoutSignatureType : int;
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
class QtHCT;

class TestSettlement : public ::testing::Test, public SignerCallbackTarget
{
protected:
   TestSettlement();
   ~TestSettlement() override;

   void SetUp() override;
   void TearDown() override;

   void mineBlocks(unsigned count, bool wait = true);
   void sendTo(uint64_t value, bs::Address& addr);

public:
   std::shared_ptr<TestEnv> envPtr_;
   SecureBinaryData passphrase_;

protected:
   const size_t   nbParties_ = 2;
   const double   initialTransferAmount_ = 1.23;
   std::vector<std::shared_ptr<bs::core::hd::Wallet>> hdWallet_;
   std::vector<std::shared_ptr<bs::core::hd::Leaf>>   authWallet_;
   std::vector<std::shared_ptr<bs::core::Wallet>>     xbtWallet_;
   std::vector<std::shared_ptr<bs::core::WalletsManager>>   walletsMgr_;
   std::vector<std::shared_ptr<WalletSignerContainer>>      inprocSigner_;
   std::vector<bs::Address>      authAddrs_;
   std::vector<SecureBinaryData> authKeys_;
   std::vector<bs::Address>      fundAddrs_, recvAddrs_, changeAddrs_;
   std::map<bs::Address, std::shared_ptr<bs::core::hd::Leaf>>  settlLeafMap_;

   const std::string fxSecurity_{ "EUR/USD" };
   const std::string fxProduct_{ "EUR" };
   const std::string xbtSecurity_{ "XBT/EUR" };

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

private:
   void onWalletReady(const QString &id);
};


#endif // __TEST_SETTLEMENT_H__
