/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TEST_AUTH_H__
#define __TEST_AUTH_H__

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
class QtHCT;

////////////////////////////////////////////////////////////////////////////////
#if 0 // Auth address code turned off
class TestValidationACT : public ValidationAddressACT
{
private:
   ArmoryThreading::BlockingQueue<std::shared_ptr<DBNotificationStruct>>
      notifTestQueue_;

public:
   TestValidationACT(ArmoryConnection *armory) :
      ValidationAddressACT(armory)
   {}

   ////
   void onRefresh(const std::vector<BinaryData> &, bool) override;
   void onZCReceived(const std::string& requestId, const std::vector<bs::TXEntry>& zcs) override;

   ////
   virtual void start() override {}
   virtual void stop() override {}

   ////
   void waitOnRefresh(const std::vector<std::string>& ids)
   {
      if (ids.size() == 0)
         throw std::runtime_error("empty registration id vector");

      std::set<std::string> idSet;
      idSet.insert(ids.begin(), ids.end());

      while (true)
      {
         auto&& notif = notifTestQueue_.pop_front();
         if (notif->type_ != DBNS_Refresh)
            throw std::runtime_error("expected refresh notification");

         for (auto& refreshId : notif->ids_)
         {
            std::string idStr(refreshId.getCharPtr(), refreshId.getSize());
            auto iter = idSet.find(idStr);
            if (iter == idSet.end())
               continue;

            idSet.erase(iter);
            if (idSet.size() == 0)
               return;
         }
      }
   }

   void waitOnZC(const BinaryData& hash)
   {
      while (true)
      {
         auto&& notif = notifTestQueue_.pop_front();
         if (notif->type_ != DBNS_ZC)
            continue;

         for (auto& zc : notif->zc_)
         {
            if (zc.txHash == hash)
               return;
         }
      }
   }
};

////////////////////////////////////////////////////////////////////////////////
class TestAuth : public ::testing::Test, public SignerCallbackTarget
{
protected:
   void SetUp() override;
   void TearDown() override;

   void mineBlocks(unsigned, bool wait = true);
   BinaryData sendTo(uint64_t, bs::Address&);
   bs::Address getNewAddress(std::shared_ptr<bs::sync::Wallet> wltPtr, bool ext);

protected:
   const double   initialAmount_ = 1.01;
   const uint32_t ccFundingAmount_ = 1000;
   const uint64_t ccLotSize_ = 526;
   std::shared_ptr<bs::core::hd::Wallet>  priWallet_;
   std::shared_ptr<bs::core::hd::Leaf>    authSignWallet_;
   std::shared_ptr<bs::sync::Wallet>   authWallet_;
   std::shared_ptr<bs::core::Wallet>   xbtSignWallet_;
   std::shared_ptr<bs::sync::Wallet>   xbtWallet_;
   std::shared_ptr<bs::sync::WalletsManager> syncMgr_;
   std::shared_ptr<QtHCT>  hct_;
   bs::Address    recvAddr_;
   std::shared_ptr<TestEnv> envPtr_;

   std::shared_ptr<TestValidationACT> actPtr_;

   SecureBinaryData coinbasePrivKey_ =
      READHEX("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
   BinaryData coinbasePubKey_;
   BinaryData coinbaseScrAddr_;
   std::shared_ptr<ResolverOneAddress> coinbaseFeed_;

   SecureBinaryData validationPrivKey_ =
      READHEX("102122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F");
   BinaryData validationPubKey_;
   BinaryData validationScrAddr_;
   std::shared_ptr<ResolverOneAddress> validationFeed_;
   bs::Address    validationAddr_;

   SecureBinaryData userID_ = 
      READHEX("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");

   std::map<unsigned, BinaryData> coinbaseHashes_;
   unsigned coinbaseCounter_ = 0;

   SecureBinaryData passphrase_;
};
#endif   //0

#endif
