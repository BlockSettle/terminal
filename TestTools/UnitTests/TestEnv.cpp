#include <atomic>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <spdlog/spdlog.h>

#include "TestEnv.h"

#include "ApplicationSettings.h"
#include "AuthAddressManager.h"
#include "BS_regtest.h"
#include "CelerClient.h"
#include "ConnectionManager.h"
#include "MarketDataProvider.h"
#include "OTPFile.h"
#include "OTPManager.h"
#include "PyBlockDataManager.h"
#include "QuoteProvider.h"
#include "UiUtils.h"
#include "WalletsManager.h"

class BlockListener : public PyBlockDataListener
{
public:
   BlockListener() : connected(false), ready(false) {}
   ~BlockListener() noexcept override = default;

   void StateChanged(PyBlockDataManagerState newState) override {
      ready = (newState == PyBlockDataManagerState::Ready);
      if (!ready) {
         connected = (newState == PyBlockDataManagerState::Connected);
      }
   }

   std::atomic_bool  connected, ready;
};


std::shared_ptr<ApplicationSettings> TestEnv::appSettings_;
std::shared_ptr<MockAssetManager> TestEnv::assetMgr_;
std::shared_ptr<MockAuthAddrMgr> TestEnv::authAddrMgr_;
std::shared_ptr<BlockchainMonitor> TestEnv::blockMonitor_;
std::shared_ptr<CelerClient> TestEnv::celerConn_;
std::shared_ptr<ConnectionManager> TestEnv::connMgr_;
std::shared_ptr<spdlog::logger> TestEnv::logger_;
std::shared_ptr<MarketDataProvider> TestEnv::mdProvider_;
std::shared_ptr<QuoteProvider> TestEnv::quoteProvider_;
std::shared_ptr<OTPManager> TestEnv::otpMgr_;
std::shared_ptr<RegtestController> TestEnv::regtestControl_;
std::shared_ptr<WalletsManager> TestEnv::walletsMgr_;

TestEnv::TestEnv(const std::shared_ptr<spdlog::logger> &logger)
{
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
   if (!appSettings_->LoadApplicationSettings({QLatin1String("unit_tests")})) {
      qDebug() << "Failed to load app settings:" << appSettings_->ErrorText();
   }

   regtestControl_ = std::make_shared<RegtestController>(BS_REGTEST_HOST, BS_REGTEST_PORT, BS_REGTEST_AUTH_COOKIE);
   TestEnv::logger()->debug("Bitcoin balance = {}", regtestControl_->GetBalance());

   bdmListener_ = std::make_shared<BlockListener>();
   bdm_ = PyBlockDataManager::createDataManager(appSettings_->GetArmorySettings(), "tx_cache");
   PyBlockDataManager::setInstance(bdm_);
   bdm_->addListener(bdmListener_.get());
   if (!bdm_->setupConnection()) {
      logger_->error("Failed to setup BDM connection");
   }
   blockMonitor_ = std::make_shared<BlockchainMonitor>();

   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   celerConn_ = std::make_shared<CelerClient>(connMgr_);

   mdProvider_ = std::make_shared<MarketDataProvider>(logger_);

   const auto otpRoot = SecureBinaryData().GenerateRandom(32);
   const auto otpFile = OTPFile::CreateFromPrivateKey(logger_, QString(), otpRoot, BinaryData("otpPass"));
   otpMgr_ = std::make_shared<OTPManager>(logger_, otpFile);

   walletsMgr_ = std::make_shared<WalletsManager>(logger_, appSettings_, bdm_, false);
   authAddrMgr_ = std::make_shared<MockAuthAddrMgr>(logger_);

   assetMgr_ = std::make_shared<MockAssetManager>(logger_);
   assetMgr_->init();

   quoteProvider_ = std::make_shared<QuoteProvider>(assetMgr_, logger_);

   qDebug() << "Waiting for ArmoryDB connection...";
   while (!bdmListener_->connected) {
      QThread::msleep(23);
   }
   qDebug() << "Armory connected - waiting for ready state...";
   bdm_->goOnline();
   while (!bdmListener_->ready) {
      QThread::msleep(23);
   }
   qDebug() << "Armory is ready - continue execution";

   logger_->debug("BDM ready");
}

void TestEnv::TearDown()
{
   logger_->debug("BS unit tests finished");
   logger_->flush();
   mdProvider_ = nullptr;
   quoteProvider_ = nullptr;
   walletsMgr_ = nullptr;
   authAddrMgr_ = nullptr;
   otpMgr_ = nullptr;
   celerConn_ = nullptr;

   QDir(appSettings_->GetHomeDir()).removeRecursively();
   QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();

   bdm_ = nullptr;
   PyBlockDataManager::setInstance(nullptr);
   assetMgr_ = nullptr;
   connMgr_ = nullptr;
   appSettings_ = nullptr;
}
