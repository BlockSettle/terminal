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
#include "CelerClient.h"
#include "ConnectionManager.h"
#include "CoreWalletsManager.h"
#include "MarketDataProvider.h"
#include "QuoteProvider.h"
#include "UiUtils.h"


std::shared_ptr<ApplicationSettings> TestEnv::appSettings_;
std::shared_ptr<ArmoryConnection> TestEnv::armoryConnection_;
std::shared_ptr<ArmoryInstance> TestEnv::armoryInstance_;
std::shared_ptr<MockAssetManager> TestEnv::assetMgr_;
std::shared_ptr<MockAuthAddrMgr> TestEnv::authAddrMgr_;
std::shared_ptr<BlockchainMonitor> TestEnv::blockMonitor_;
std::shared_ptr<CelerClient> TestEnv::celerConn_;
std::shared_ptr<ConnectionManager> TestEnv::connMgr_;
std::shared_ptr<spdlog::logger> TestEnv::logger_;
std::shared_ptr<MarketDataProvider> TestEnv::mdProvider_;
std::shared_ptr<QuoteProvider> TestEnv::quoteProvider_;
std::shared_ptr<bs::core::WalletsManager> TestEnv::walletsMgr_;

const BinaryData testnetGenesisBlock = READHEX("0100000000000000000000000000000000000\
000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51\
323a9fb8aa4b1e5e4adae5494dffff001d1aa4ae18010100000001000000000000000000000000000\
0000000000000000000000000000000000000ffffffff4d04ffff001d0104455468652054696d6573\
2030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6\
e64206261696c6f757420666f722062616e6b73ffffffff0100f2052a01000000434104678afdb0fe\
5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec\
112de5c384df7ba0b8d578a4c702b6bf11d5fac00000000");

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

   armoryConnection_ = nullptr;
   armoryInstance_ = nullptr;
   assetMgr_ = nullptr;
   connMgr_ = nullptr;
   appSettings_ = nullptr;
}

void TestEnv::requireArmory()
{
   if (regtestControl_) {
      return;
   }

   //init armorydb
   armoryInstance_ = std::make_shared<ArmoryInstance>();
   regtestControl_ = std::make_shared<RegtestController>(BS_REGTEST_HOST, BS_REGTEST_PORT, BS_REGTEST_AUTH_COOKIE);

   armoryConnection_ = std::make_shared<ArmoryConnection>(logger_, "tx_cache");
   ArmorySettings settings;
   settings.runLocally = false;
   settings.socketType = TestEnv::appSettings()->GetArmorySocketType();
   settings.netType = NetworkType::TestNet;
   settings.armoryDBIp = QLatin1String("127.0.0.1");
   settings.armoryDBPort = armoryInstance_->port_;
   settings.dataDir = QLatin1String("armory_regtest_db");
   armoryConnection_->setupConnection(settings, [](const BinaryData &, const std::string &) { return true; });

   blockMonitor_ = std::make_shared<BlockchainMonitor>(armoryConnection_);

   connMgr_ = std::make_shared<ConnectionManager>(logger_);
   celerConn_ = std::make_shared<CelerClient>(connMgr_);

   qDebug() << "Waiting for ArmoryDB connection...";
   while (armoryConnection_->state() != ArmoryConnection::State::Connected) {
      QThread::msleep(23);
   }
   qDebug() << "Armory connected - waiting for ready state...";
   armoryConnection_->goOnline();
   while (armoryConnection_->state() != ArmoryConnection::State::Ready) {
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
   authAddrMgr_ = std::make_shared<MockAuthAddrMgr>(logger_, armoryConnection_);

   assetMgr_ = std::make_shared<MockAssetManager>(logger_);
   assetMgr_->init();

   mdProvider_ = std::make_shared<MarketDataProvider>(logger_);
   quoteProvider_ = std::make_shared<QuoteProvider>(assetMgr_, logger_);
}

///////////////////////////////////////////////////////////////////////////////
ArmoryInstance::ArmoryInstance()
{
   //setup armory folders
   blkdir_ = std::string("./blkfiletest");
   homedir_ = std::string("./fakehomedir");
   ldbdir_ = std::string("./ldbtestdir");

   DBUtils::removeDirectory(blkdir_);
   DBUtils::removeDirectory(homedir_);
   DBUtils::removeDirectory(ldbdir_);

   mkdir(blkdir_.c_str());
   mkdir(homedir_.c_str());
   mkdir(ldbdir_.c_str());

   //setup env
   NetworkConfig::selectNetwork(NETWORK_MODE_TESTNET);
   BlockDataManagerConfig::setServiceType(SERVICE_WEBSOCKET);
   BlockDataManagerConfig::setDbType(ARMORY_DB_SUPER);
   auto& magicBytes = NetworkConfig::getMagicBytes();

   //create block file with testnet genesis block
   auto blk0dat = BtcUtils::getBlkFilename(blkdir_, 0);
   std::ofstream fs(blk0dat, std::ios::binary);

   //testnet magic word
   fs.write(magicBytes.getCharPtr(), 4);

   //block size
   uint32_t blockSize = testnetGenesisBlock.getSize();
   fs.write((char*)&blockSize, 4);

   //testnet genesis block
   fs.write((const char*)testnetGenesisBlock.getPtr(), blockSize);
   fs.close();

   //setup config
   config_.blkFileLocation_ = blkdir_;
   config_.dbDir_ = ldbdir_;
   config_.threadCount_ = 3;
   config_.dataDir_ = homedir_;
   config_.ephemeralPeers_ = false;

   port_ = 50000 + rand() % 10000;
   std::stringstream port_ss;
   port_ss << port_;
   config_.listenPort_ = port_ss.str();

   //setup bip151 context
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   //setup auth
   AuthorizedPeers serverPeers(homedir_, SERVER_AUTH_PEER_FILENAME);
   AuthorizedPeers clientPeers(homedir_, CLIENT_AUTH_PEER_FILENAME);

   auto& serverPubkey = serverPeers.getOwnPublicKey();
   auto& clientPubkey = clientPeers.getOwnPublicKey();

   std::stringstream serverAddr;
   serverAddr << "127.0.0.1:" << config_.listenPort_;
   clientPeers.addPeer(serverPubkey, serverAddr.str());
   serverPeers.addPeer(clientPubkey, "127.0.0.1");

   //init bdm
   nodePtr_ =
      std::make_shared<NodeUnitTest>(*(unsigned int*)magicBytes.getPtr());
   config_.nodePtr_ = std::dynamic_pointer_cast<BitcoinP2P>(nodePtr_);

   theBDMt_ = new BlockDataManagerThread(config_);
   iface_ = theBDMt_->bdm()->getIFace();

   auto nodePtr = std::dynamic_pointer_cast<NodeUnitTest>(config_.nodePtr_);
   nodePtr->setBlockchain(theBDMt_->bdm()->blockchain());
   nodePtr->setBlockFiles(theBDMt_->bdm()->blockFiles());

   theBDMt_->start(config_.initMode_);

   //start server
   WebSocketServer::start(theBDMt_, BlockDataManagerConfig::getDataDir(),
      BlockDataManagerConfig::ephemeralPeers_, true);
}

ArmoryInstance::~ArmoryInstance()
{
   //shutdown server
   auto&& bdvObj2 = AsyncClient::BlockDataViewer::getNewBDV(
      "127.0.0.1", config_.listenPort_, BlockDataManagerConfig::getDataDir(),
      BlockDataManagerConfig::ephemeralPeers_, nullptr);
   auto&& serverPubkey = WebSocketServer::getPublicKey();
   bdvObj2->addPublicKey(serverPubkey);
   bdvObj2->connectToRemote();

   bdvObj2->shutdown(config_.cookie_);
   WebSocketServer::waitOnShutdown();

   //shutdown bdm
   delete theBDMt_;
   theBDMt_ = nullptr;

   //clean up dirs
   DBUtils::removeDirectory(blkdir_);
   DBUtils::removeDirectory(homedir_);
   DBUtils::removeDirectory(ldbdir_);
}

void ArmoryInstance::mineNewBlock(const BinaryData& addr)
{
   nodePtr_->mineNewBlock(addr);
}