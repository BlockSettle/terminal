/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "TestEnv.h"

#include "ApplicationSettings.h"
#include "ArmoryConfig.h"
#include "AuthorizedPeers.h"
#include "Wallets/ArmoryObject.h"
#include "ArmorySettings.h"
#include "ConnectionManager.h"
#include "CoreWalletsManager.h"
#include "MarketDataProvider.h"
#include "MDCallbacksQt.h"
#include "SystemFileUtils.h"
#include "UiUtils.h"

#include <atomic>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <spdlog/spdlog.h>
#include <btc/ecc.h>

using namespace Armory::Signer;
using namespace Armory::Wallets;

const BinaryData testnetGenesisBlock = READHEX("0100000000000000000000000000000000000\
000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51\
323a9fb8aa4b1e5e4adae5494dffff001d1aa4ae18010100000001000000000000000000000000000\
0000000000000000000000000000000000000ffffffff4d04ffff001d0104455468652054696d6573\
2030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6\
e64206261696c6f757420666f722062616e6b73ffffffff0100f2052a01000000434104678afdb0fe\
5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec\
112de5c384df7ba0b8d578a4c702b6bf11d5fac00000000");

std::shared_ptr<spdlog::logger> StaticLogger::loggerPtr = nullptr;
Armory::Threading::TimedQueue<std::shared_ptr<DBNotificationStruct>> ACTqueue::notifQueue_;

TestEnv::TestEnv(const std::shared_ptr<spdlog::logger> &logger)
{
   QStandardPaths::setTestModeEnabled(true);
   logger_ = logger;
   UiUtils::SetupLocale();

   appSettings_ = std::make_shared<ApplicationSettings>(QLatin1String("BS_unit_tests"));
   appSettings_->set(ApplicationSettings::netType, (int)NetworkType::RegTest);

   /*   appSettings_->set(ApplicationSettings::armoryDbIp, QLatin1String("localhost"));
   appSettings_->set(ApplicationSettings::armoryDbPort, 19001);*/
   appSettings_->set(ApplicationSettings::armoryDbIp, QLatin1String("127.0.0.1"));
   appSettings_->set(ApplicationSettings::armoryDbPort, 82);
   appSettings_->set(ApplicationSettings::initialized, true);
   if (!appSettings_->LoadApplicationSettings({ QLatin1String("unit_tests") })) {
      qDebug() << "Failed to load app settings:" << appSettings_->ErrorText();
   }

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger_, 0);
}

void TestEnv::shutdown()
{
   if (appSettings_ != nullptr)
      QDir(appSettings_->GetHomeDir()).removeRecursively();

   if (logger_ != nullptr) {
      logger_->flush();
      logger_ = nullptr;
   }

   mdProvider_ = nullptr;

   assetMgr_ = nullptr;
   connMgr_ = nullptr;
   appSettings_ = nullptr;

   walletsMgr_ = nullptr;

   armoryInstance_.reset();
   armoryConnection_.reset();

   QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();

   ACTqueue::notifQueue_.clear();
}

void TestEnv::requireArmory(bool waitForReady)
{
   //init armorydb
   if (armoryInstance_ != nullptr) {
      armoryInstance_->init();
      return;
   }
   armoryInstance_ = std::make_shared<ArmoryInstance>();

   auto armoryConnection = std::make_shared<TestArmoryConnection>(
      armoryInstance_, logger_, "");
   ArmorySettings settings;
   settings.runLocally = false;
   settings.socketType = appSettings()->GetArmorySocketType();
   settings.netType = NetworkType::TestNet;
   settings.armoryDBIp = QLatin1String("127.0.0.1");
   settings.armoryDBPort = armoryInstance_->port_;
   settings.dataDir = QLatin1String("armory_regtest_db");

   const auto& keyCb = [](const BinaryData&, const std::string&)->bool
   {
      return true;
   };
   const auto& cbConnected = [this, armoryConnection, waitForReady]
   {
      armoryConnection_ = armoryConnection;
      blockMonitor_ = std::make_shared<BlockchainMonitor>(armoryConnection_);
      armoryConnection_->goOnline();
   };
   const auto& cbConnFailed = []
   {
      ASSERT_TRUE(false) << "ArmoryDB connection failed";
   };
   armoryConnection->setupConnection(settings, cbConnected, cbConnFailed, keyCb);

   if (waitForReady) {
      qDebug() << "Waiting for ArmoryDB connection...";
      while (armoryConnection->state() != ArmoryState::Ready) {
         std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
      }
      logger_->debug("Armory is ready - continue execution");
   }
}

void TestEnv::requireAssets()
{
   requireConnections();

   assetMgr_ = std::make_shared<MockAssetManager>(logger_);
   assetMgr_->init();

   mdCallbacks_ = std::make_shared<MDCallbacksQt>();
   mdProvider_ = std::make_shared<MarketDataProvider>(logger_, mdCallbacks_.get());
}

void TestEnv::requireConnections()
{
   requireArmory();

   connMgr_ = std::make_shared<ConnectionManager>(logger_);
}

///////////////////////////////////////////////////////////////////////////////
ArmoryInstance::ArmoryInstance()
   : blkdir_("./blkfiletest"), homedir_("./fakehomedir"), ldbdir_("./ldbtestdir")
{
   init();
}

void ArmoryInstance::init()
{
   //setup armory folders
   SystemFileUtils::rmDir(blkdir_);
   SystemFileUtils::rmDir(homedir_);
   SystemFileUtils::rmDir(ldbdir_);

   SystemFileUtils::mkPath(blkdir_);
   SystemFileUtils::mkPath(homedir_);
   SystemFileUtils::mkPath(ldbdir_);

   //setup env
   Armory::Config::NetworkSettings::selectNetwork(Armory::Config::NETWORK_MODE_TESTNET);
   Armory::Config::DBSettings::setServiceType(SERVICE_WEBSOCKET);
   auto& magicBytes = Armory::Config::BitcoinSettings::getMagicBytes();

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
   port_ = UNITTEST_DB_PORT;

   //setup bip151 context
   startupBIP150CTX(4);

   std::vector<std::string> args{ "--db-type=DB_SUPER", "--public"
      , "--satoshi-datadir=" + blkdir_, "--dbdir=" + ldbdir_, "--datadir=" + homedir_
      , "--thread-count=3", "--listen-port=" + std::to_string(port_) };
   Armory::Config::parseArgs(args, Armory::Config::ProcessType::DB);

   const auto lbdEmptyPassphrase = [](const std::set<Armory::Wallets::EncryptionKeyId>&) {
      return SecureBinaryData{};
   };

   //setup auth
   AuthorizedPeers serverPeers(homedir_, SERVER_AUTH_PEER_FILENAME, lbdEmptyPassphrase);
   AuthorizedPeers clientPeers(homedir_, CLIENT_AUTH_PEER_FILENAME, lbdEmptyPassphrase);

   auto& serverPubkey = serverPeers.getOwnPublicKey();
   auto& clientPubkey = clientPeers.getOwnPublicKey();

   std::stringstream serverAddr;
   serverAddr << "127.0.0.1:" << port_;
   clientPeers.addPeer(serverPubkey, serverAddr.str());
   serverPeers.addPeer(clientPubkey, "127.0.0.1");

   //init bdm & network nodes
   nodePtr_ = std::dynamic_pointer_cast<NodeUnitTest>(
      Armory::Config::NetworkSettings::bitcoinNodes().first);

   nodePtr_ = std::dynamic_pointer_cast<NodeUnitTest>(
      Armory::Config::NetworkSettings::bitcoinNodes().first);

   rpcNode_ = std::dynamic_pointer_cast<NodeRPC_UnitTest>(
      Armory::Config::NetworkSettings::rpcNode());

   theBDMt_ = new BlockDataManagerThread();
   iface_ = theBDMt_->bdm()->getIFace();

   nodePtr_->setBlockchain(theBDMt_->bdm()->blockchain());
   nodePtr_->setBlockFiles(theBDMt_->bdm()->blockFiles());
   nodePtr_->setIface(iface_);

   theBDMt_->start(INIT_RESUME);

   //start server
   WebSocketServer::initAuthPeers(lbdEmptyPassphrase);
   WebSocketServer::start(theBDMt_, true);
}

////
void ArmoryInstance::shutdown()
{
   if (theBDMt_ == nullptr) {
      return;
   }

   //shutdown server
   auto&& bdvObj2 = AsyncClient::BlockDataViewer::getNewBDV(
      "127.0.0.1", std::to_string(port_), Armory::Config::getDataDir()
      , [](const std::set<Armory::Wallets::EncryptionKeyId>&) { return SecureBinaryData{}; }
   , Armory::Config::NetworkSettings::ephemeralPeers(), true, nullptr);
   auto&& serverPubkey = WebSocketServer::getPublicKey();
   bdvObj2->addPublicKey(serverPubkey);
   bdvObj2->connectToRemote();

   bdvObj2->shutdown(Armory::Config::NetworkSettings::cookie());
   WebSocketServer::waitOnShutdown();

   //shutdown bdm
   delete theBDMt_;
   theBDMt_ = nullptr;
   nodePtr_ = nullptr;

   //clean up dirs
   SystemFileUtils::rmDir(blkdir_);
   SystemFileUtils::rmDir(homedir_);
   SystemFileUtils::rmDir(ldbdir_);
}

std::map<unsigned, BinaryData> ArmoryInstance::mineNewBlock(
   ScriptRecipient* rec, unsigned count)
{
   return nodePtr_->mineNewBlock(theBDMt_->bdm(), count, rec);
}

void ArmoryInstance::pushZC(const BinaryData& zc, unsigned int blocksUntilMined, bool stage)
{
   std::vector<std::pair<BinaryData, unsigned int>> zcVec;
   zcVec.push_back({ zc, blocksUntilMined });
   nodePtr_->pushZC(zcVec, stage);
}

void ArmoryInstance::setReorgBranchPoint(const BinaryData& hash)
{
   auto headerPtr = theBDMt_->bdm()->blockchain()->getHeaderByHash(hash);
   if (headerPtr == nullptr) {
      throw std::runtime_error("null header ptr");
   }
   nodePtr_->setReorgBranchPoint(headerPtr);
}

BinaryData ArmoryInstance::getCurrentTopBlockHash() const
{
   auto headerPtr = theBDMt_->bdm()->blockchain()->top();
   if (headerPtr == nullptr) {
      throw std::runtime_error("null header ptr");
   }
   return headerPtr->getThisHash();
}

uint32_t ArmoryInstance::getCurrentTopBlock(void) const
{
   auto headerPtr = theBDMt_->bdm()->blockchain()->top();
   if (headerPtr == nullptr) {
      throw std::runtime_error("null header ptr");
   }
   return headerPtr->getBlockHeight();
}


////
SingleUTWalletACT::~SingleUTWalletACT()
{
   cleanup();
}

void SingleUTWalletACT::onRefresh(const std::vector<BinaryData> &ids, bool online)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
   dbns->ids_ = ids;
   dbns->online_ = online;

   ACTqueue::notifQueue_.push_back(std::move(dbns));
}

void SingleUTWalletACT::onZCReceived(const std::string& requestId, const std::vector<bs::TXEntry>& zcs)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
   dbns->zc_ = zcs;

   ACTqueue::notifQueue_.push_back(std::move(dbns));
}

void SingleUTWalletACT::onNewBlock(unsigned int block, unsigned int)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_NewBlock);
   dbns->block_ = block;

   ACTqueue::notifQueue_.push_back(std::move(dbns));
}

void UnitTestWalletACT::onTxBroadcastError(const std::string &reqId, const BinaryData &txHash
   , int errCode, const std::string &errMsg)
{
   bs::sync::WalletACT::onTxBroadcastError(reqId, txHash, errCode, errMsg);

   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_TxBroadcastError);
   dbns->zc_ = { { txHash } };
   dbns->requestId_ = reqId;
   dbns->errCode_ = errCode;

   ACTqueue::notifQueue_.push_back(std::move(dbns));
}

std::vector<bs::TXEntry> UnitTestWalletACT::waitOnZC(bool soft)
{
   while (true) {
      try {
         const auto &notif = ACTqueue::notifQueue_.pop_front(std::chrono::seconds{ 10 });
         if (notif->type_ != DBNS_ZC) {
            if (soft) {
               continue;
            }
            throw std::runtime_error("expected zc notification");
         }
         return notif->zc_;
      }
      catch (const Armory::Threading::StackTimedOutException &) {
         return {};
      }
   }
}

int UnitTestWalletACT::waitOnBroadcastError(const std::string &reqId)
{
   try {
      const auto &notif = ACTqueue::notifQueue_.pop_front(std::chrono::seconds{ 10 });
      if (notif->type_ != DBNS_TxBroadcastError) {
         return 1;
      }
      if (notif->requestId_ != reqId) {
         return 1;
      }
      return notif->errCode_;
   }
   catch (const Armory::Threading::StackTimedOutException &) {
      return 0;
   }
}

bs::Address randomAddressPKH()
{
   auto privKey = CryptoPRNG::generateRandom(32);
   auto pubkey = CryptoECDSA().ComputePublicKey(privKey, true);
   auto pubkeyHash = BtcUtils::getHash160(pubkey.getRef());
   auto addr = bs::Address::fromPubKey(pubkey, AddressEntryType_P2PKH);
   return addr;
}
