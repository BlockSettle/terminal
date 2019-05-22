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

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

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

class ResolverCoinbase : public ResolverFeed
{
private:
   SecureBinaryData privKey_;
   BinaryData pubKey_;

public:
   ResolverCoinbase(
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
   std::shared_ptr<ArmoryObject> armoryConnection() { return armoryConnection_; }
   std::shared_ptr<ArmoryInstance> armoryInstance() { return armoryInstance_; }
   std::shared_ptr<MockAssetManager> assetMgr() { return assetMgr_; }
   std::shared_ptr<MockAuthAddrMgr> authAddrMgr() { return authAddrMgr_; }
   std::shared_ptr<BlockchainMonitor> blockMonitor() { return blockMonitor_; }
   std::shared_ptr<ConnectionManager> connectionMgr() { return connMgr_; }
   std::shared_ptr<CelerClient> celerConnection() { return celerConn_; }
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
   std::shared_ptr<CelerClient>          celerConn_;
   std::shared_ptr<ConnectionManager>    connMgr_;
   std::shared_ptr<MarketDataProvider>   mdProvider_;
   std::shared_ptr<QuoteProvider>        quoteProvider_;
   std::shared_ptr<bs::core::WalletsManager>       walletsMgr_;
   std::shared_ptr<spdlog::logger>       logger_;
   std::shared_ptr<ArmoryObject>     armoryConnection_;
   std::shared_ptr<ArmoryInstance>       armoryInstance_;
};

#endif // __TEST_ENV_H__
