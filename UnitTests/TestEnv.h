#ifndef __TEST_ENV_H__
#define __TEST_ENV_H__

#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "BlockchainMonitor.h"
#include "MockAssetMgr.h"
#include "MockAuthAddrMgr.h"
#include "Server.h"
#include "gtest/NodeUnitTest.h"
#include "BlockDataManagerConfig.h"
#include "BDM_mainthread.h"
#include <btc/ecc.h>

#include "ArmoryObject.h"
#include "Wallets/SyncWallet.h"
#include "AuthAddressLogic.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#define UNITTEST_DB_PORT 59095

struct StaticLogger
{
   static std::shared_ptr<spdlog::logger> loggerPtr;
};

namespace bs {
   namespace core {
      class WalletsManager;
   }
}
class ApplicationSettings;
class ArmoryConnection;
class AuthAddressManager;
class BlockchainMonitor;
class CelerClient;
class ConnectionManager;
class MarketDataProvider;
class QuoteProvider;

class ResolverOneAddress : public ResolverFeed
{
private:
   SecureBinaryData privKey_;
   BinaryData pubKey_;
   BinaryData hash_;

public:
   ResolverOneAddress(
      const SecureBinaryData& privkey,
      const BinaryData& pubkey) :
      privKey_(privkey), pubKey_(pubkey)
   {
      hash_ = BtcUtils::hash160(pubKey_);
   }

   BinaryData getByVal(const BinaryData& hash)
   {
      if(hash != hash_)
         throw std::runtime_error("no pubkey for this hash");
         
      return pubKey_;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey)
   {
      if(pubkey != pubKey_)
         throw std::runtime_error("no privkey for this pubkey");

      return privKey_;
   }
};

class ResolverManyAddresses : public ResolverFeed
{
private:
   std::map<BinaryData, SecureBinaryData> hashToPubKey_;
   std::map<SecureBinaryData, SecureBinaryData> pubKeyToPriv_;

public:
   ResolverManyAddresses(std::set<SecureBinaryData> privKeys)
   {
      for (auto& privKey : privKeys)
      {
         auto&& pubKey = CryptoECDSA().ComputePublicKey(privKey, true);
         auto&& hash = BtcUtils::getHash160(pubKey);

         hashToPubKey_.insert(std::make_pair(hash, pubKey));
         pubKeyToPriv_.insert(std::make_pair(pubKey, privKey));
      }
   }

   BinaryData getByVal(const BinaryData& hash)
   {
      auto iter = hashToPubKey_.find(hash);
      if (iter == hashToPubKey_.end())
         throw std::runtime_error("no pubkey for this hash");

      return iter->second;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey)
   {
      auto iter = pubKeyToPriv_.find(pubkey);
      if (iter == pubKeyToPriv_.end())
         throw std::runtime_error("no privkey for this pubkey");

      return iter->second;
   }

};


struct ACTqueue {
   static ArmoryThreading::BlockingQueue<std::shared_ptr<DBNotificationStruct>> notifQueue_;
};

class SingleUTWalletACT : public ArmoryCallbackTarget
{
public:
   SingleUTWalletACT(ArmoryConnection *armory)
      : ArmoryCallbackTarget()
   {
      init(armory);
   }
   ~SingleUTWalletACT() override;

   void onRefresh(const std::vector<BinaryData> &ids, bool online) override;
   void onZCReceived(const std::vector<bs::TXEntry> &zcs) override;
   void onNewBlock(unsigned int block, unsigned int branchHgt) override;
};

class UnitTestWalletACT : public bs::sync::WalletACT
{
//   static BlockingQueue<std::shared_ptr<DBNotificationStruct>> notifQueue_;

public:
   UnitTestWalletACT(ArmoryConnection *armory, bs::sync::Wallet *leaf) :
      bs::sync::WalletACT(leaf)
   {
      init(armory);
   }
   ~UnitTestWalletACT() override
   {
      cleanup();
   }

   void onRefresh(const std::vector<BinaryData> &ids, bool online) override
   {
      bs::sync::WalletACT::onRefresh(ids, online);

      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
      dbns->ids_ = ids;
      dbns->online_ = online;

      ACTqueue::notifQueue_.push_back(std::move(dbns));
   }

   void onZCReceived(const std::vector<bs::TXEntry> &zcs) override
   {
      bs::sync::WalletACT::onZCReceived(zcs);

      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
      dbns->zc_ = zcs;

      ACTqueue::notifQueue_.push_back(std::move(dbns));
   }

   void onNewBlock(unsigned int block, unsigned int bh) override
   {
      bs::sync::WalletACT::onNewBlock(block, bh);

      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_NewBlock);
      dbns->block_ = block;

      ACTqueue::notifQueue_.push_back(std::move(dbns));
   }
   
   static std::shared_ptr<DBNotificationStruct> waitOnNotification(void)
   {
      return ACTqueue::notifQueue_.pop_front();
   }

   static void waitOnRefresh(const std::vector<std::string>& ids, bool strict = true)
   {
      if (ids.size() == 0) {
         throw std::runtime_error("empty registration id vector");
      }
      std::set<std::string> idSet;
      idSet.insert(ids.begin(), ids.end());

      while (true) {
         const auto &notif = ACTqueue::notifQueue_.pop_front();
         if (notif->type_ != DBNS_Refresh) {
            if (strict) {
               throw std::runtime_error("expected refresh notification");
            }
            else {
               continue;
            }
         }
         for (auto& refreshId : notif->ids_) {
            std::string idStr(refreshId.getCharPtr(), refreshId.getSize());
            auto iter = idSet.find(idStr);
            if (iter == idSet.end()) {
               continue;
            }
            idSet.erase(iter);
            if (idSet.size() == 0) {
               return;
            }
         }
      }
   }

   static unsigned waitOnNewBlock(bool strict = false)
   {
      while (true) {
         auto&& notif = ACTqueue::notifQueue_.pop_front();
         if (notif->type_ != DBNS_NewBlock) {
            if (strict) {
               throw std::runtime_error("expected new block notification (got " + std::to_string(notif->type_) + ")");
            } else {
               continue;
            }
         }
         return notif->block_;
      }
   }

   static std::vector<bs::TXEntry> waitOnZC(bool soft = false);

   static std::shared_ptr<DBNotificationStruct> popNotif()
   {
      return ACTqueue::notifQueue_.pop_front();
   }

   //to clear the notification queue
   static void clear(void)
   {
      ACTqueue::notifQueue_.clear();
   }
};

struct UnitTestLocalACT : public bs::sync::WalletACT
{
   ArmoryThreading::BlockingQueue<std::shared_ptr<DBNotificationStruct>> notifQueue_;

public:
   UnitTestLocalACT(ArmoryConnection *armory, bs::sync::Wallet *leaf) :
      bs::sync::WalletACT(leaf)
   {
      init(armory);
   }
   ~UnitTestLocalACT() override
   {
      cleanup();
   }

   void onRefresh(const std::vector<BinaryData> &ids, bool online) override
   {
      bs::sync::WalletACT::onRefresh(ids, online);

      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
      dbns->ids_ = ids;
      dbns->online_ = online;

      notifQueue_.push_back(std::move(dbns));
   }

   void onZCReceived(const std::vector<bs::TXEntry> &zcs) override
   {
      bs::sync::WalletACT::onZCReceived(zcs);

      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
      dbns->zc_ = zcs;

      notifQueue_.push_back(std::move(dbns));
   }

   void onNewBlock(unsigned int block, unsigned branchHeight) override
   {
      bs::sync::WalletACT::onNewBlock(block, branchHeight);

      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_NewBlock);
      dbns->block_ = block;
      dbns->branchHeight_ = branchHeight;

      notifQueue_.push_back(std::move(dbns));
   }

   std::shared_ptr<DBNotificationStruct> waitOnNotification(void)
   {
      return std::move(notifQueue_.pop_front());
   }

   void waitOnRefresh(const std::vector<std::string>& ids)
   {
      if (ids.size() == 0) {
         throw std::runtime_error("empty registration id vector");
      }
      std::set<std::string> idSet;
      idSet.insert(ids.begin(), ids.end());

      while (true) {
         const auto &notif = notifQueue_.pop_front();
         if (notif->type_ != DBNS_Refresh) {
            throw std::runtime_error("expected refresh notification");
         }
         for (auto& refreshId : notif->ids_) {
            std::string idStr(refreshId.getCharPtr(), refreshId.getSize());
            auto iter = idSet.find(idStr);
            if (iter == idSet.end()) {
               continue;
            }
            idSet.erase(iter);
            if (idSet.size() == 0) {
               return;
            }
         }
      }
   }

   unsigned waitOnNewBlock()
   {
      auto&& notif = notifQueue_.pop_front();
      if (notif->type_ != DBNS_NewBlock)
         throw std::runtime_error("expected new block notification (got " + std::to_string(notif->type_) + ")");

      return notif->block_;
   }

   std::vector<bs::TXEntry> waitOnZC(bool soft = false)
   {
      while (1)
      {
         auto&& notif = notifQueue_.pop_front();
         if (notif->type_ != DBNS_ZC)
         {
            if (soft)
               continue;

            throw std::runtime_error("expected zc notification");
         }

         return notif->zc_;
      }
   }

   std::shared_ptr<DBNotificationStruct> popNotif()
   {
      return notifQueue_.pop_front();
   }
};

struct ArmoryInstance
{
   /*in process supernode db running off of spoofed unit test network node*/

   const std::string blkdir_;
   const std::string homedir_;
   const std::string ldbdir_;
   int port_;

   std::shared_ptr<NodeUnitTest> nodePtr_;

   BlockDataManagerConfig config_;

   BlockDataManagerThread* theBDMt_;
   LMDBBlockDatabase* iface_;

   ArmoryInstance();
   ~ArmoryInstance(void);

   std::map<unsigned, BinaryData> mineNewBlock(ScriptRecipient*, unsigned);
   void pushZC(const BinaryData &, unsigned int blocksUntilMined = 0, bool stage = false);

   void setReorgBranchPoint(const BinaryData&);
   BinaryData getCurrentTopBlockHash(void) const;
};

class TestArmoryConnection : public ArmoryObject
{
   std::shared_ptr<ArmoryInstance> armoryInstance_;

public:
   TestArmoryConnection(
      std::shared_ptr<ArmoryInstance> armoryInstance,
      const std::shared_ptr<spdlog::logger> &loggerRef,
      const std::string &txCacheFN, 
      bool cbInMainThread = true) :
      ArmoryObject(loggerRef, txCacheFN, cbInMainThread)
      , armoryInstance_(armoryInstance)
   {}

   static float testFeePerByte()
   {
      return 1.0f;
   }

   bool pushZC(const BinaryData& rawTx) const override
   {
      if (armoryInstance_ == nullptr)
         return false;

      armoryInstance_->pushZC(rawTx);
      return true;
   }

   bool estimateFee(unsigned int nbBlocks, const FloatCb &cb) override
   {
      std::thread([cb] {
         cb(float(1000 * double(testFeePerByte()) / BTCNumericTypes::BalanceDivider));
      }).detach();
      return true;
   }

   std::shared_ptr<AsyncClient::BlockDataViewer> bdv() const { return bdv_; }
};

class TestEnv
{
public:
   TestEnv(const std::shared_ptr<spdlog::logger> &);
   ~TestEnv(void) { shutdown(); }

   void shutdown(void);
   
   std::shared_ptr<ApplicationSettings> appSettings() { return appSettings_; }
   std::shared_ptr<TestArmoryConnection> armoryConnection() { return armoryConnection_; }
   std::shared_ptr<ArmoryInstance> armoryInstance() { return armoryInstance_; }
   std::shared_ptr<MockAssetManager> assetMgr() { return assetMgr_; }
   std::shared_ptr<MockAuthAddrMgr> authAddrMgr() { return authAddrMgr_; }
   std::shared_ptr<BlockchainMonitor> blockMonitor() { return blockMonitor_; }
   std::shared_ptr<ConnectionManager> connectionMgr() { return connMgr_; }
   std::shared_ptr<BaseCelerClient> celerConnection() { return celerConn_; }
   std::shared_ptr<spdlog::logger> logger() { return logger_; }
   std::shared_ptr<bs::core::WalletsManager> walletsMgr() { return walletsMgr_; }
   std::shared_ptr<MarketDataProvider> mdProvider() { return mdProvider_; }
   std::shared_ptr<QuoteProvider> quoteProvider() { return quoteProvider_; }

   void requireArmory();
   void requireAssets();
   void requireConnections();

private:
   std::shared_ptr<ApplicationSettings>  appSettings_;
   std::shared_ptr<MockAssetManager>     assetMgr_;
   std::shared_ptr<MockAuthAddrMgr>      authAddrMgr_;
   std::shared_ptr<BlockchainMonitor>    blockMonitor_;
   std::shared_ptr<BaseCelerClient>      celerConn_;
   std::shared_ptr<ConnectionManager>    connMgr_;
   std::shared_ptr<MarketDataProvider>   mdProvider_;
   std::shared_ptr<QuoteProvider>        quoteProvider_;
   std::shared_ptr<bs::core::WalletsManager>       walletsMgr_;
   std::shared_ptr<spdlog::logger>       logger_;
   std::shared_ptr<TestArmoryConnection> armoryConnection_;
   std::shared_ptr<ArmoryInstance>       armoryInstance_;
};

#endif // __TEST_ENV_H__
