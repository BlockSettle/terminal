#include <atomic>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <spdlog/spdlog.h>
#include <btc/ecc.h>

#include "TestEnv.h"

#include "ApplicationSettings.h"
#include "ArmoryObject.h"
#include "ArmorySettings.h"
#include "AuthAddressManager.h"
#include "BS_regtest.h"
#include "CelerClient.h"
#include "ConnectionManager.h"
#include "CoreWalletsManager.h"
#include "MarketDataProvider.h"
#include "QuoteProvider.h"
#include "UiUtils.h"


std::shared_ptr<ApplicationSettings> TestEnv::appSettings_;
std::shared_ptr<ArmoryObject> TestEnv::armory_;
std::shared_ptr<MockAssetManager> TestEnv::assetMgr_;
std::shared_ptr<MockAuthAddrMgr> TestEnv::authAddrMgr_;
std::shared_ptr<BlockchainMonitor> TestEnv::blockMonitor_;
std::shared_ptr<CelerClient> TestEnv::celerConn_;
std::shared_ptr<ConnectionManager> TestEnv::connMgr_;
std::shared_ptr<spdlog::logger> TestEnv::logger_;
std::shared_ptr<MarketDataProvider> TestEnv::mdProvider_;
std::shared_ptr<QuoteProvider> TestEnv::quoteProvider_;
std::shared_ptr<RegtestController> TestEnv::regtestControl_;
std::shared_ptr<bs::core::WalletsManager> TestEnv::walletsMgr_;

TestEnv::TestEnv(const std::shared_ptr<spdlog::logger> &logger)
{
   std::srand(std::time(nullptr));

   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   QStandardPaths::setTestModeEnabled(true);
   logger_ = logger;
   UiUtils::SetupLocale();

   appSettings_ = std::make_shared<ApplicationSettings>(QLatin1String("BS_unit_tests"));
   appSettings_->set(ApplicationSettings::netType, (int)NetworkType::RegTest);

   /*   appSettings_->set(ApplicationSettings::armoryDbIp, QLatin1String("localhost"));
   appSettings_->set(ApplicationSettings::armoryDbPort, 19001);*/
   appSettings_->set(ApplicationSettings::armoryDbIp, QLatin1String("193.138.218.38"));
   appSettings_->set(ApplicationSettings::armoryDbPort, 82);
   appSettings_->set(ApplicationSettings::initialized, true);
   if (!appSettings_->LoadApplicationSettings({ QLatin1String("unit_tests") })) {
      qDebug() << "Failed to load app settings:" << appSettings_->ErrorText();
   }

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger_, 0);
}

void TestEnv::TearDown()
{
   logger_->debug("BS unit tests finished");
   logger_->flush();
   mdProvider_ = nullptr;
   quoteProvider_ = nullptr;
   walletsMgr_ = nullptr;
   authAddrMgr_ = nullptr;
   celerConn_ = nullptr;

   QDir(appSettings_->GetHomeDir()).removeRecursively();
   QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();

   armory_ = nullptr;
   assetMgr_ = nullptr;
   connMgr_ = nullptr;
   appSettings_ = nullptr;
}

void TestEnv::requireArmory()
{
   if (regtestControl_) {
      return;
   }
   regtestControl_ = std::make_shared<RegtestController>(BS_REGTEST_HOST, BS_REGTEST_PORT, BS_REGTEST_AUTH_COOKIE);
   const auto &cbBalance = [](double balance) {
      logger_->debug("Bitcoin balance = {}", balance);
   };
   regtestControl_->GetBalance(cbBalance);

   armory_ = std::make_shared<ArmoryObject>(logger_, "tx_cache");
   ArmorySettings settings;
   settings.runLocally = false;
   settings.socketType = TestEnv::appSettings()->GetArmorySocketType();
   settings.netType = NetworkType::RegTest;
   settings.armoryDBIp = TestEnv::appSettings()->get<QString>(ApplicationSettings::armoryDbIp);
   settings.armoryDBPort = TestEnv::appSettings()->get<int>(ApplicationSettings::armoryDbPort);
   settings.dataDir = QLatin1String("armory_regtest_db");
   armory_->setupConnection(settings, [](const BinaryData &, const std::string &) { return true; });

   blockMonitor_ = std::make_shared<BlockchainMonitor>(armory_);

   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   celerConn_ = std::make_shared<CelerClient>(connMgr_);

   qDebug() << "Waiting for ArmoryDB connection...";
   while (armory_->state() != ArmoryConnection::State::Connected) {
      QThread::msleep(23);
   }
   qDebug() << "Armory connected - waiting for ready state...";
   armory_->goOnline();
   while (armory_->state() != ArmoryConnection::State::Ready) {
      QThread::msleep(23);
   }
   logger_->debug("Armory is ready - continue execution");
}

void TestEnv::requireAssets()
{
   if (authAddrMgr_ && assetMgr_) {
      return;
   }
   requireArmory();
   authAddrMgr_ = std::make_shared<MockAuthAddrMgr>(logger_, armory_);

   assetMgr_ = std::make_shared<MockAssetManager>(logger_);
   assetMgr_->init();

   mdProvider_ = std::make_shared<MarketDataProvider>(logger_);
   quoteProvider_ = std::make_shared<QuoteProvider>(assetMgr_, logger_);
}
