#ifndef __TEST_ENV_H__
#define __TEST_ENV_H__

#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "BlockchainMonitor.h"
#include "MockAssetMgr.h"
#include "MockAuthAddrMgr.h"
#include "RegtestController.h"
#include "Server.h"
#include "gtest/NodeUnitTest.h"
#include "BlockDataManagerConfig.h"
#include "BDM_mainthread.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
}
class ApplicationSettings;
class ArmoryObject;
class AuthAddressManager;
class BlockchainMonitor;
class CelerClient;
class ConnectionManager;
class MarketDataProvider;
class QuoteProvider;

struct ArmoryInstance
{
   /*in process supernode db running off of spoofed unit test network node*/

   std::string blkdir_;
   std::string homedir_;
   std::string ldbdir_;
   int port_;

   std::shared_ptr<NodeUnitTest> nodePtr_;

   BlockDataManagerConfig config_;

   BlockDataManagerThread* theBDMt_;
   LMDBBlockDatabase* iface_;

   ArmoryInstance();
   ~ArmoryInstance(void);

   void mineNewBlock(const BinaryData& addr);
};

class TestEnv : public testing::Environment
{
public:
   TestEnv(const std::shared_ptr<spdlog::logger> &);
   
   void TearDown() override;

   static std::shared_ptr<ApplicationSettings> appSettings() { return appSettings_; }
   static std::shared_ptr<ArmoryConnection> armoryConnection() { return armoryConnection_; }
   static std::shared_ptr<ArmoryInstance> armoryInstance() { return armoryInstance_; }
   static std::shared_ptr<MockAssetManager> assetMgr() { return assetMgr_; }
   static std::shared_ptr<MockAuthAddrMgr> authAddrMgr() { return authAddrMgr_; }
   static std::shared_ptr<BlockchainMonitor> blockMonitor() { return blockMonitor_; }
   static std::shared_ptr<ConnectionManager> connectionMgr() { return connMgr_; }
   static std::shared_ptr<CelerClient> celerConnection() { return celerConn_; }
   static std::shared_ptr<spdlog::logger> logger() { return logger_; }
   static std::shared_ptr<bs::core::WalletsManager> walletsMgr() { return walletsMgr_; }
   static std::shared_ptr<MarketDataProvider> mdProvider() { return mdProvider_; }
   static std::shared_ptr<QuoteProvider> quoteProvider() { return quoteProvider_; }

   static void requireArmory();
   static void requireAssets();

private:
   static std::shared_ptr<ApplicationSettings>  appSettings_;
   static std::shared_ptr<MockAssetManager>     assetMgr_;
   static std::shared_ptr<MockAuthAddrMgr>      authAddrMgr_;
   static std::shared_ptr<BlockchainMonitor>    blockMonitor_;
   static std::shared_ptr<CelerClient>          celerConn_;
   static std::shared_ptr<ConnectionManager>    connMgr_;
   static std::shared_ptr<MarketDataProvider>   mdProvider_;
   static std::shared_ptr<QuoteProvider>        quoteProvider_;
   static std::shared_ptr<bs::core::WalletsManager>       walletsMgr_;
   static std::shared_ptr<spdlog::logger>       logger_;
   static std::shared_ptr<ArmoryConnection>     armoryConnection_;
   static std::shared_ptr<ArmoryInstance>       armoryInstance_;
};

#endif // __TEST_ENV_H__
