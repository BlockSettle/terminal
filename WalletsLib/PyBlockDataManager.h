#ifndef __BLOCK_DATA_MANAGER_H__
#define __BLOCK_DATA_MANAGER_H__

#include <atomic>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <QProcess>
#include <QMutex>

#include "ArmorySettings.h"
#include "SwigClient.h"
#include "CacheFile.h"


class ManualResetEvent;
class SafeBtcWallet;
class SafeLedgerDelegate;

enum class PyBlockDataManagerState : int
{
   Offline,
   Connected,
   Scaning,
   Error,
   Closing,
   Ready
};

Q_DECLARE_METATYPE(PyBlockDataManagerState);

class PyBlockDataListener
{
public:
   PyBlockDataListener() = default;
   virtual ~PyBlockDataListener() noexcept = default;

   virtual void StateChanged(PyBlockDataManagerState ) {}
   virtual void ProgressUpdated(BDMPhase , const vector<string> &, float, unsigned , unsigned ) {}
   virtual void OnRefresh() {}
   virtual void OnNewBlock(uint32_t) {}
};

class BlockDataListenerSignalAdapter : public QObject, public PyBlockDataListener
{
   Q_OBJECT

public:
   BlockDataListenerSignalAdapter(QObject* parent = nullptr);
   ~BlockDataListenerSignalAdapter() noexcept override = default;

   BlockDataListenerSignalAdapter(const BlockDataListenerSignalAdapter&) = delete;
   BlockDataListenerSignalAdapter& operator = (const BlockDataListenerSignalAdapter&) = delete;

   BlockDataListenerSignalAdapter(BlockDataListenerSignalAdapter&&) = delete;
   BlockDataListenerSignalAdapter& operator = (BlockDataListenerSignalAdapter&&) = delete;

   void StateChanged(PyBlockDataManagerState newState) override;

signals:
   void OnStateChanged(PyBlockDataManagerState newState);
};

class PyBlockDataManager : public QObject, public SwigClient::PythonCallback
{
   Q_OBJECT
public:
   static std::shared_ptr<PyBlockDataManager> createDataManager(const ArmorySettings &, const std::string &txCacheFN);

   PyBlockDataManager(const std::shared_ptr<SwigClient::BlockDataViewer> &, const ArmorySettings &
      , const std::string &txCacheFN);
   ~PyBlockDataManager() noexcept override;

   PyBlockDataManager(const PyBlockDataManager&) = delete;
   PyBlockDataManager& operator = (const PyBlockDataManager&) = delete;

   PyBlockDataManager(PyBlockDataManager&&) = delete;
   PyBlockDataManager& operator = (PyBlockDataManager&&) = delete;

   void run(BDMAction action, void* ptr, int block=0) override;
   void progress( BDMPhase phase, const vector<string> &walletIdVec, float progress,
                  unsigned secondsRem, unsigned progressNumeric) override;

   // NOTE: at this point collection is not concurrent, there should not be additions and
   // notification processing from different threads.
   void addListener(PyBlockDataListener* listener);
   void removeListener(PyBlockDataListener* listener);

   bool setupConnection();
   bool goOnline();

   void registerWallet(std::shared_ptr<SafeBtcWallet> &, std::vector<BinaryData> const& scrAddrVec, std::string ID, bool wltIsNew);

   PyBlockDataManagerState GetState() const;

   uint32_t GetTopBlockHeight() const;

   std::shared_ptr<SafeLedgerDelegate> GetWalletsLedgerDelegate();
   std::shared_ptr<SafeLedgerDelegate> getLedgerDelegateForScrAddr(const string& walletID, const BinaryData& scrAddr);
   std::vector<LedgerEntryData> getWalletsHistory(const std::vector<std::string> &walletIDs);

   bool broadcastZC(const BinaryData& rawTx);

   bool IgnoreAllZC() const;

   static std::shared_ptr<PyBlockDataManager> instance();
   static void setInstance(std::shared_ptr<PyBlockDataManager> bdm);

   Tx getTxByHash(const BinaryData& hash);
   bool IsTransactionVerified(const LedgerEntryData& item);
   bool IsTransactionConfirmed(const LedgerEntryData& item);

   int  GetConfirmationsNumber(const LedgerEntryData& item);

   float estimateFee(unsigned int nbBlocks);

   NodeStatusStruct getNodeStatus();

   bool updateWalletsLedgerFilter(const vector<BinaryData>& wltIdVec);

   void OnConnectionError();

signals:
   void zeroConfReceived(const std::vector<LedgerEntryData>&);
   void txBroadcastError(const QString &txHash, const QString &error);
   void refreshed(const BinaryDataVector &ids);
   void newBlock();

   void ScheduleRPCBroadcast(const BinaryData& rawTx);

   // goingOffline sent after state is changed. So you either react to signal, or to listener notification
   void goingOffline();

public slots:
   void onShutdownConnection();
   void onScheduleRPCBroadcast(const BinaryData& rawTx);

private:
   void registerBDV();
   void SetState(PyBlockDataManagerState newState);

   bool isArmoryDBAvailable();
   bool StartLocalArmoryDB();

   bool OnArmoryAvailable();

   bool startConnectionMonitor();
   bool stopConnectionMonitor();

   void monitoringRoutine();

   void onZCReceived(const std::vector<LedgerEntryData> &);
   void onNewBlock();
   void onRefresh(const BinaryDataVector &ids);

private:
   ArmorySettings                         settings_;
   std::vector<PyBlockDataListener*>      listeners_;

   PyBlockDataManagerState                currentState_;
   std::shared_ptr<SwigClient::BlockDataViewer> bdv_;

   uint32_t                               topBlockHeight_;

   std::shared_ptr<QProcess>              armoryProcess_;

   std::thread                            connectionMonitorThread_;
   std::shared_ptr<ManualResetEvent>      stopMonitorEvent_;

   mutable QMutex                         lsnMtx_;

   TxCacheFile                            txCache_;
   mutable std::atomic_flag  bdvLock_ = ATOMIC_FLAG_INIT;

   std::unordered_set<BinaryData>         pendingBroadcasts_;
};

#endif // __BLOCK_DATA_MANAGER_H__
