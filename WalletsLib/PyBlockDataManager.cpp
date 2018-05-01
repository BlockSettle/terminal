#include "PyBlockDataManager.h"

#include <QFile>
#include <QProcess>
#include <QDebug>
#include <QMutexLocker>
#include <QTimer>

#include <cassert>
#include <exception>

#include "DbHeader.h"
#include "EncryptionUtils.h"
#include "FastLock.h"
#include "JSON_codec.h"
#include "ManualResetEvent.h"
#include "SafeBtcWallet.h"
#include "SafeLedgerDelegate.h"
#include "SocketIncludes.h"

const int DefaultArmoryDBStartTimeoutMsec = 500;
constexpr uint64_t CheckConnectionTimeoutMiliseconds = 500;

static std::shared_ptr<PyBlockDataManager> globalInstance = nullptr;

std::shared_ptr<PyBlockDataManager> PyBlockDataManager::createDataManager(const ArmorySettings& settings, const std::string &txCacheFN)
{
   return std::make_shared<PyBlockDataManager>(std::make_shared<SwigClient::BlockDataViewer>(SwigClient::BlockDataViewer::getNewBDV(settings.armoryDBIp
      , settings.armoryDBPort, settings.socketType)), settings, txCacheFN);
}

BlockDataListenerSignalAdapter::BlockDataListenerSignalAdapter(QObject* parent)
   : QObject(parent)
{}

void BlockDataListenerSignalAdapter::StateChanged(PyBlockDataManagerState newState)
{
   emit OnStateChanged(newState);
}

PyBlockDataManager::PyBlockDataManager(const std::shared_ptr<SwigClient::BlockDataViewer> &bdv, const ArmorySettings& settings, const std::string &txCacheFN)
   : PythonCallback(bdv.get())
   , settings_(settings)
   , currentState_(PyBlockDataManagerState::Offline)
   , bdv_(bdv)
   , topBlockHeight_(0)
   , armoryProcess_(nullptr)
   , txCache_(txCacheFN)
{
   stopMonitorEvent_ = std::make_shared<ManualResetEvent>();

   connect(this, &PyBlockDataManager::ScheduleRPCBroadcast, this
      , &PyBlockDataManager::onScheduleRPCBroadcast, Qt::QueuedConnection);
}

void PyBlockDataManager::registerBDV()
{
   BinaryData magicBytes;
   switch (settings_.netType) {
   case NetworkType::TestNet:
      magicBytes = READHEX(TESTNET_MAGIC_BYTES);
      break;
   case NetworkType::RegTest:
      magicBytes = READHEX(REGTEST_MAGIC_BYTES);
      break;
   case NetworkType::MainNet:
      magicBytes = READHEX(MAINNET_MAGIC_BYTES);
      break;
   default:
      throw std::runtime_error("unknown network type");
   }

   FastLock lock(bdvLock_);
   bdv_->registerWithDB(magicBytes);
}

PyBlockDataManager::~PyBlockDataManager() noexcept
{
   if (currentState_ == PyBlockDataManagerState::Ready || currentState_ == PyBlockDataManagerState::Scaning) {
      try {
         FastLock lock(bdvLock_);
         bdv_->unregisterFromDB();
      }
      catch (...) { }
   }

   stopConnectionMonitor();
}

void PyBlockDataManager::run(BDMAction action, void* ptr, int block)
{
   switch(action) {
   case BDMAction_Ready:
      topBlockHeight_ = block;
      SetState(PyBlockDataManagerState::Ready);
      qDebug() << "BDMAction_Ready";
      break;
   case BDMAction_NewBlock:
      topBlockHeight_ = block;
      qDebug() << "BDMAction_NewBlock";
      // force update
      SetState(PyBlockDataManagerState::Ready);
      onNewBlock();
      break;
   case BDMAction_ZC:
      qDebug() << "BDMAction_ZC";
      onZCReceived(*reinterpret_cast<std::vector<LedgerEntryData>*>(ptr));
      break;
   case BDMAction_Refresh:
      qDebug() << "BDMAction_Refresh";
      onRefresh(*reinterpret_cast<BinaryDataVector *>(ptr));
      break;
   case BDMAction_Exited:
      // skip for now
      break;
   case BDMAction_ErrorMsg:
      // skip for now
      break;
   case BDMAction_NodeStatus:
   {
      NodeStatusStruct *nss = (NodeStatusStruct*)ptr;
      QString nodeStatusString;
      QString rpcStatusString;
      QString chainStatusString;

      switch(nss->status_)
      {
      case NodeStatus_Offline:
         nodeStatusString = QLatin1String("Offline");
         break;
      case NodeStatus_Online:
         nodeStatusString = QLatin1String("Online");
         break;
      case NodeStatus_OffSync:
         nodeStatusString = QLatin1String("OffSync");
         break;
      }

      switch(nss->rpcStatus_)
      {
      case RpcStatus_Disabled:
         rpcStatusString = QLatin1String("Disabled");
         break;
      case RpcStatus_BadAuth:
         rpcStatusString = QLatin1String("BadAuth");
         break;
      case RpcStatus_Online:
         rpcStatusString = QLatin1String("Online");
         break;
      case RpcStatus_Error_28:
         rpcStatusString = QLatin1String("Error_28");
         break;
      }

      switch(nss->chainState_.state())
      {
      case ChainStatus_Unknown:
         chainStatusString = QLatin1String("Unknown");
         break;
      case ChainStatus_Syncing:
         chainStatusString = QLatin1String("Syncing");
         break;
      case ChainStatus_Ready:
         chainStatusString = QLatin1String("Ready");
         break;
      }

      QString segwitEnabled = nss->SegWitEnabled_ ? QLatin1String("Enabled") : QLatin1String("Disabled");

      qDebug() << "Node status : " << nodeStatusString << "\n"
               << "RPC status  : " << rpcStatusString << "\n"
               << "Chain status: " << chainStatusString << "\n"
               << "SegWit      : " << segwitEnabled;
   }
      break;

   case BDMAction_BDV_Error:
      BDV_Error_Struct *bdvErr = (BDV_Error_Struct *)ptr;
      qDebug() << "BDMAction_BDV_Error: " << bdvErr->errType_ << " " << QString::fromStdString(bdvErr->errorStr_) << "\n"
         << QString::fromStdString(bdvErr->extraMsg_) << "\n";

      switch (bdvErr->errType_)
      {
      case Error_ZC:
         emit txBroadcastError(QString::fromStdString(bdvErr->extraMsg_), QString::fromStdString(bdvErr->errorStr_));
         break;
      }
   }
}

void PyBlockDataManager::onNewBlock()
{
   {
      QMutexLocker lock(&lsnMtx_);
      for (const auto &listener : listeners_) {
         listener->OnNewBlock(topBlockHeight_);
      }
   }
   emit newBlock();
}

void PyBlockDataManager::onRefresh(const BinaryDataVector &ids)
{
   emit refreshed(ids);
   QMutexLocker lock(&lsnMtx_);
   for (auto listener : listeners_) {
      listener->OnRefresh();
   }
}

void PyBlockDataManager::progress( BDMPhase phase, const vector<string> &walletIdVec, float progress,
  unsigned secondsRem, unsigned progressNumeric)
{
   QMutexLocker lock(&lsnMtx_);
   for(auto listener : listeners_) {
      listener->ProgressUpdated(phase, walletIdVec, progress, secondsRem, progressNumeric);
   }
}

PyBlockDataManagerState PyBlockDataManager::GetState() const
{
   return currentState_;
}

void PyBlockDataManager::SetState(PyBlockDataManagerState newState)
{
   if (newState != currentState_) {
      qDebug() << "Set state: " << (int)newState;
      currentState_ = newState;
      QMutexLocker lock(&lsnMtx_);
      for(auto listener : listeners_) {
         listener->StateChanged(currentState_);
      }
   }
}

bool PyBlockDataManager::setupConnection()
{
   if (currentState_ != PyBlockDataManagerState::Offline) {
      return false;
   }

   if (!isArmoryDBAvailable() && settings_.runLocally) {
     qDebug() << QLatin1String("DB is offline");
     if (!StartLocalArmoryDB()) {
        qDebug() << QLatin1String("Failed to start DB");

        SetState(PyBlockDataManagerState::Offline);
        return false;
     }
   }

   return startConnectionMonitor();
}

bool PyBlockDataManager::goOnline()
{
   if (GetState() != PyBlockDataManagerState::Connected) {
      return false;
   }

   try {
      FastLock lock(bdvLock_);
      bdv_->goOnline();
//      startLoop();
      const auto loop = [this](void)->void {
         try {
            this->remoteLoop();
         }
         catch (const DbErrorMsg &) {
            OnConnectionError();
         }
         catch (...) {
            OnConnectionError();
         }
      };

      thr_ = std::thread(loop);
   }
   catch (runtime_error &)
   {
      OnConnectionError();
      return false;
   }

   return true;
}

void PyBlockDataManager::registerWallet(std::shared_ptr<SafeBtcWallet> &wallet, std::vector<BinaryData> const& scrAddrVec, std::string ID, bool wltIsNew)
{
   try {
      FastLock lock(bdvLock_);
      if (!wallet) {
         wallet = std::make_shared<SafeBtcWallet>(bdv_->registerWallet(ID, scrAddrVec, wltIsNew), bdvLock_);
      }
      else {
         bdv_->registerWallet(ID, scrAddrVec, wltIsNew);
      }
   }
   catch (const std::exception &e) {
      qDebug() << "registerWallet exception:" << QString::fromStdString(e.what());
   }
   catch (const DbErrorMsg &e) {
      qDebug() << "registerWallet DB exception:" << QString::fromStdString(e.what());
   }
   catch (...) {
      qDebug() << "registerWallet unknown exception";
   }
}

uint32_t PyBlockDataManager::GetTopBlockHeight() const
{
   return topBlockHeight_;
}

void PyBlockDataManager::addListener(PyBlockDataListener* listener)
{
   QMutexLocker lock(&lsnMtx_);
   listeners_.push_back(listener);
}

void PyBlockDataManager::removeListener(PyBlockDataListener* listener)
{
   QMutexLocker lock(&lsnMtx_);
   auto it = std::find(listeners_.begin(), listeners_.end(), listener);
   if (it != listeners_.end()) {
      listeners_.erase(it);
   }
}

std::shared_ptr<SafeLedgerDelegate> PyBlockDataManager::GetWalletsLedgerDelegate()
{
   try {
      FastLock lock(bdvLock_);
      return std::make_shared<SafeLedgerDelegate>(bdv_->getLedgerDelegateForWallets(), bdvLock_);
   } catch (const SocketError& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::GetWalletsLedgerDelegate] SocketError: ") << QString::fromStdString(e.what());
   } catch (const DbErrorMsg& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::GetWalletsLedgerDelegate] DbErrorMsg: ") << QString::fromStdString(e.what());
   } catch (const std::exception& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::GetWalletsLedgerDelegate] exception: ") << QString::fromStdString(e.what());
   } catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::GetWalletsLedgerDelegate] exception.");
   }

   OnConnectionError();
   return nullptr;
}

std::shared_ptr<SafeLedgerDelegate> PyBlockDataManager::getLedgerDelegateForScrAddr(const string& walletID, const BinaryData& scrAddr)
{
   try {
      FastLock lock(bdvLock_);
      return std::make_shared<SafeLedgerDelegate>(bdv_->getLedgerDelegateForScrAddr(walletID, scrAddr), bdvLock_);
   } catch (const SocketError& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::getLedgerDelegateForScrAddr] SocketError: ") << QString::fromStdString(e.what());
   } catch (const DbErrorMsg& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::getLedgerDelegateForScrAddr] DbErrorMsg: ") << QString::fromStdString(e.what());
      return nullptr;   // most likely it's just a DB error, so no reason to reconnect
   } catch (const std::exception& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::getLedgerDelegateForScrAddr] exception: ") << QString::fromStdString(e.what());
   } catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::getLedgerDelegateForScrAddr] exception.");
   }

   OnConnectionError();
   return nullptr;
}

std::vector<LedgerEntryData> PyBlockDataManager::getWalletsHistory(const std::vector<std::string> &walletIDs)
{
   try {
      FastLock lock(bdvLock_);
      return bdv_->getHistoryForWalletSelection(walletIDs, "ascending");
   }
   catch (const DbErrorMsg &e) {
      qDebug() << QLatin1String("[PyBlockDataManager::getWalletsHistory] DbErrorMsg: ") << QString::fromStdString(e.what());
   }
   catch (const std::exception &e) {
      qDebug() << QLatin1String("[PyBlockDataManager::getWalletsHistory] exception: ") << QLatin1String(e.what());
   }
   catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::getWalletsHistory] exception");
   }
   return {};
}

bool PyBlockDataManager::IgnoreAllZC() const
{
   return settings_.ignoreAllZC;
}

bool PyBlockDataManager::isArmoryDBAvailable()
{
   FastLock lock(bdvLock_);
   return bdv_->hasRemoteDB();
}

bool PyBlockDataManager::StartLocalArmoryDB()
{
   const QString armoryDBPath = settings_.armoryExecutablePath;
   if (QFile::exists(armoryDBPath)) {
      armoryProcess_ = std::make_shared<QProcess>();

      QStringList args;
      switch (settings_.netType) {
      case NetworkType::TestNet:
         args.append(QString::fromStdString("--testnet"));
         break;
      case NetworkType::RegTest:
         args.append(QString::fromStdString("--regtest"));
         break;
      default: break;
      }

      std::string spawnID = SecureBinaryData().GenerateRandom(32).toHexStr();
      args.append(QLatin1String("--spawnId=\"") + QString::fromStdString(spawnID) + QLatin1String("\""));
      args.append(QLatin1String("--satoshi-datadir=\"") + settings_.bitcoinBlocksDir + QLatin1String("\""));
      args.append(QLatin1String("--dbdir=\"") + settings_.dbDir + QLatin1String("\""));

      armoryProcess_->start(settings_.armoryExecutablePath, args);
      if (armoryProcess_->waitForStarted(DefaultArmoryDBStartTimeoutMsec)) {
         return true;
      }
      armoryProcess_.reset();
   }
   return false;
}

void PyBlockDataManager::setInstance(std::shared_ptr<PyBlockDataManager> bdm)
{
   globalInstance = bdm;
}

std::shared_ptr<PyBlockDataManager> PyBlockDataManager::instance()
{
   return globalInstance;
}

Tx PyBlockDataManager::getTxByHash(const BinaryData& hash)
{
   auto tx = txCache_.get(hash);
   if (tx.isInitialized()) {
      return tx;
   }
   try {
      {
         FastLock lock(bdvLock_);
         tx = bdv_->getTxByHash(hash);
      }
      if (tx.isInitialized()) {
         txCache_.put(hash, tx);
      }
      return tx;
   } catch (const std::exception &e) {
      qDebug() << QLatin1String("[PyBlockDataManager::getTxByHash] exception") << QLatin1String(e.what());
   } catch(const DbErrorMsg &) {
      qDebug() << "DB error";
   }

   return Tx{};
}

int PyBlockDataManager::GetConfirmationsNumber(const LedgerEntryData& item)
{
   if (item.getBlockNum() < uint32_t(-1)) {
      return GetTopBlockHeight() + 1 - item.getBlockNum();
   }

   return 0;
}

bool PyBlockDataManager::IsTransactionVerified(const LedgerEntryData& item)
{
   return GetConfirmationsNumber(item) >= 6;
}

bool PyBlockDataManager::IsTransactionConfirmed(const LedgerEntryData& item)
{
   return GetConfirmationsNumber(item) > 1;
}

float PyBlockDataManager::estimateFee(unsigned int nbBlocks)
{
   try {
      FastLock lock(bdvLock_);
      const auto feeResult = bdv_->estimateFee(nbBlocks, FEE_STRAT_CONSERVATIVE);
      if (!feeResult.error_.empty()) {
         throw std::runtime_error(feeResult.error_);
      }
      return feeResult.val_;
   }
   catch (const std::exception &e) {
      qDebug() << "[PyBlockDataManager::estimateFee] exception:" << QLatin1String(e.what());
   }
   catch (const DbErrorMsg &e) {
      qDebug() << "[PyBlockDataManager::estimateFee] DB exception:" << QString::fromStdString(e.what());
      if (e.what() == "conf_target mismatch") { // special handling for large nbBlocks (testnet problem?)
         return 0.00001;
      }
   } catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::estimateFee] exception.");
   }
   return 0;
}

NodeStatusStruct PyBlockDataManager::getNodeStatus()
{
   try {
      FastLock lock(bdvLock_);
      return bdv_->getNodeStatus();
   } catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::getNodeStatus] exception.");
   }

   OnConnectionError();

   return NodeStatusStruct{};
}

void PyBlockDataManager::OnConnectionError()
{
   qDebug() << "[PyBlockDataManager::OnConnectionError] Get connection error notification";

   QMetaObject::invokeMethod(this, "onShutdownConnection", Qt::QueuedConnection);
}

void PyBlockDataManager::onShutdownConnection()
{
   if (currentState_ != PyBlockDataManagerState::Offline) {
      shutdown();
      SetState(PyBlockDataManagerState::Offline);
      emit goingOffline();
      startConnectionMonitor();
   }
}

void PyBlockDataManager::onZCReceived(const std::vector<LedgerEntryData> &entries)
{
   {
      FastLock lock(bdvLock_);
      for (const auto &entry : entries) {
         pendingBroadcasts_.erase(entry.getTxHash());
      }
   }
   emit zeroConfReceived(entries);
}

void PyBlockDataManager::onScheduleRPCBroadcast(const BinaryData& rawTx)
{
   Tx tx(rawTx);

   if (tx.getThisHash().isNull()) {
      qDebug() << "[PyBlockDataManager::onScheduleRPCBroadcast] TX hash null";
   }

   QTimer::singleShot(30000, [this, tx, rawTx] {
      {
         FastLock lock(bdvLock_);
         if (pendingBroadcasts_.find(tx.getThisHash()) == pendingBroadcasts_.end()) {
            return;
         }
         pendingBroadcasts_.erase(tx.getThisHash());
      }

      const auto &result = bdv_->broadcastThroughRPC(rawTx);
      if (result != "success") {
         qDebug() << "[PyBlockDataManager::onScheduleRPCBroadcast] broadcast error";
         emit txBroadcastError(QString::fromStdString(tx.getThisHash().toHexStr(true))
            , QString::fromStdString(result));
      } else {
         qDebug() << "[PyBlockDataManager::onScheduleRPCBroadcast] broadcasted through RPC";
      }
   });
}

bool PyBlockDataManager::broadcastZC(const BinaryData& rawTx)
{
   try {
      Tx tx(rawTx);
      auto hash = tx.getThisHash();

      if (hash.isNull()) {
         qDebug() << "[PyBlockDataManager::broadcastZC] TX hash null";
      }

      {
         FastLock lock(bdvLock_);
         pendingBroadcasts_.insert(tx.getThisHash());
      }

      bdv_->broadcastZC(rawTx);

      emit ScheduleRPCBroadcast(rawTx);
      return true;
   } catch (const SocketError& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] SocketError: ") << QString::fromStdString(e.what());
   } catch (const DbErrorMsg& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] DbErrorMsg: ") << QString::fromStdString(e.what());
   } catch (const std::exception& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] exception: ") << QString::fromStdString(e.what());
   } catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] exception.");
   }

   OnConnectionError();

   return false;
}

bool PyBlockDataManager::OnArmoryAvailable()
{
   try {
      registerBDV();

      SetState(PyBlockDataManagerState::Connected);
   }
   catch (const std::exception &) {
      return false;
   }
   catch (const DbErrorMsg &) {
      return false;
   }

   return true;
}

bool PyBlockDataManager::startConnectionMonitor()
{
   stopMonitorEvent_->ResetEvent();
   if (connectionMonitorThread_.joinable()) {
      connectionMonitorThread_.join();
   }
   connectionMonitorThread_ = std::thread(&PyBlockDataManager::monitoringRoutine, this);

   return true;
}

bool PyBlockDataManager::stopConnectionMonitor()
{
   stopMonitorEvent_->SetEvent();
   if (connectionMonitorThread_.joinable()) {
      connectionMonitorThread_.join();
   }

   return true;
}

void PyBlockDataManager::monitoringRoutine()
{
   do {
      if (isArmoryDBAvailable()) {
         if (OnArmoryAvailable()) {
            // ok, we got connected, no need for monitoring
            break;
         }
      }
   } while (!stopMonitorEvent_->WaitForEvent(CheckConnectionTimeoutMiliseconds));
   qDebug() << "Quit monitoring";
}

bool PyBlockDataManager::updateWalletsLedgerFilter(const vector<BinaryData>& wltIdVec)
{
   try {
      FastLock lock(bdvLock_);
      bdv_->updateWalletsLedgerFilter(wltIdVec);
      return true;
   }
   catch (const SocketError& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] SocketError: ") << QString::fromStdString(e.what());
   }
   catch (const DbErrorMsg& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] DbErrorMsg: ") << QString::fromStdString(e.what());
   }
   catch (const std::exception& e) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] exception: ") << QString::fromStdString(e.what());
   }
   catch (...) {
      qDebug() << QLatin1String("[PyBlockDataManager::broadcastZC] exception.");
   }

   OnConnectionError();

   return false;
}
