#ifndef __TEST_ENV_H__
#define __TEST_ENV_H__

#include <memory>
#include <gtest/gtest.h>
#include "BlockchainMonitor.h"
#include "MockAssetMgr.h"
#include "MockAuthAddrMgr.h"
#include "RegtestController.h"


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

class TestEnv : public testing::Environment
{
public:
   TestEnv(const std::shared_ptr<spdlog::logger> &);
   
   void TearDown() override;

   static std::shared_ptr<ApplicationSettings> appSettings() { return appSettings_; }
   static std::shared_ptr<ArmoryObject> armory() { return armory_; }
   static std::shared_ptr<MockAssetManager> assetMgr() { return assetMgr_; }
   static std::shared_ptr<MockAuthAddrMgr> authAddrMgr() { return authAddrMgr_; }
   static std::shared_ptr<BlockchainMonitor> blockMonitor() { return blockMonitor_; }
   static std::shared_ptr<ConnectionManager> connectionMgr() { return connMgr_; }
   static std::shared_ptr<CelerClient> celerConnection() { return celerConn_; }
   static std::shared_ptr<spdlog::logger> logger() { return logger_; }
   static std::shared_ptr<RegtestController> regtestControl() { return regtestControl_; }
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
   static std::shared_ptr<RegtestController>    regtestControl_;
   static std::shared_ptr<bs::core::WalletsManager>       walletsMgr_;
   static std::shared_ptr<spdlog::logger>       logger_;
   static std::shared_ptr<ArmoryObject>     armory_;
};

#endif // __TEST_ENV_H__
