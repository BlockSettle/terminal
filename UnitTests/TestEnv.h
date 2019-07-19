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

#include "Wallets/SyncWallet.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#define UNITTEST_DB_PORT 59055

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

public:
   ResolverOneAddress(
      const SecureBinaryData& privkey,
      const BinaryData& pubkey) :
      privKey_(privkey), pubKey_(pubkey)
   {}

   BinaryData getByVal(const BinaryData& hash)
   {
      return pubKey_;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey)
   {
      return privKey_;
   }
};

enum DBNotificationStruct_Enum
{
   DBNS_Refresh,
   DBNS_ZC,
   DBNS_NewBlock
};

struct DBNotificationStruct
{
   const DBNotificationStruct_Enum type_;

   std::vector<BinaryData> ids_;
   bool online_;

   std::vector<bs::TXEntry> zc_;

   unsigned block_;

   DBNotificationStruct(DBNotificationStruct_Enum type) : 
      type_(type)
   {}
};

class UnitTestWalletACT : public bs::sync::WalletACT
{
   static BlockingQueue<std::shared_ptr<DBNotificationStruct>> notifQueue_;

public:
   UnitTestWalletACT(ArmoryConnection *armory, bs::sync::Wallet *leaf) :
      bs::sync::WalletACT(armory, leaf)
   {}

   void onRefresh(const std::vector<BinaryData> &ids, bool online) override
   {
      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
      dbns->ids_ = ids;
      dbns->online_ = online;

      notifQueue_.push_back(std::move(dbns));
   }

   void onZCReceived(const std::vector<bs::TXEntry> &zcs) override
   {
      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
      dbns->zc_ = zcs;

      notifQueue_.push_back(std::move(dbns));
   }

   void onNewBlock(unsigned int block) override
   {
      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_NewBlock);
      dbns->block_ = block;

      notifQueue_.push_back(std::move(dbns));
   }
   
   static std::shared_ptr<DBNotificationStruct> waitOnNotification(void)
   {
      return std::move(notifQueue_.pop_front());
   }

   static void waitOnRefresh(const std::vector<std::string>& ids)
   {
      if (ids.size() == 0)
         throw std::runtime_error("empty registration id vector");

      std::set<std::string> idSet;
      idSet.insert(ids.begin(), ids.end());
      
      while (true)
      {
         auto&& notif = notifQueue_.pop_front();
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

   static unsigned waitOnNewBlock()
   {
      auto&& notif = notifQueue_.pop_front();
      if(notif->type_ != DBNS_NewBlock)
         throw std::runtime_error("expected new block notification");
      
      return notif->block_;
   }

   static std::vector<bs::TXEntry> waitOnZC(bool soft = false)
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

   static std::shared_ptr<DBNotificationStruct> popNotif()
   {
      return notifQueue_.pop_front();
   }

   //to clear the notification queue
   static void clear(void)
   {
      notifQueue_.clear();
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
   void pushZC(const BinaryData&);
};

class TestEnv
{
public:
   TestEnv(const std::shared_ptr<spdlog::logger> &);
   ~TestEnv(void) { shutdown(); }

   void shutdown(void);
   
   std::shared_ptr<ApplicationSettings> appSettings() { return appSettings_; }
   std::shared_ptr<ArmoryConnection> armoryConnection() { return armoryConnection_; }
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
   std::shared_ptr<ArmoryConnection>     armoryConnection_;
   std::shared_ptr<ArmoryInstance>       armoryInstance_;
};

#endif // __TEST_ENV_H__
