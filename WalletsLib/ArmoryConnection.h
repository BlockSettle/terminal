#ifndef __ARMORY_CONNECTION_H__
#define __ARMORY_CONNECTION_H__

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QProcess>
#include <QMutex>
#include <spdlog/logger.h>

#include "Address.h"
#include "ArmorySettings.h"
#include "AsyncClient.h"
#include "CacheFile.h"
#include "BlockObj.h"

class ArmoryConnection;
class QProcess;

// Define the BIP 150 public keys used by servers controlled by BS. For dev
// purposes, they'll be hard-coded for now. THESE MUST BE REPLACED EVENTUALLY
// WITH THE KEY ROTATION ALGORITHM. HARD-CODED KEYS WILL KILL ANY TERMINAL ONCE
// THE KEYS ROTATE.
// armory.blocksettle.com - 185.213.153.37 server
//#define TESTNET_ARMORY_BLOCKSETTLE_NAME "BlockSettle TestNet Server"
//#define TESTNET_ARMORY_BLOCKSETTLE_KEY "03a8649b32b9459961e143c5c111b9a47ffa494116791c1cb35945a8b9bc8254ab"
//#define TESTNET_ARMORY_BLOCKSETTLE_ADDRESS "armory.blocksettle.com"
//#define TESTNET_ARMORY_BLOCKSETTLE_PORT 81 //7681

#define MAINNET_ARMORY_BLOCKSETTLE_NAME "BlockSettle MainNet Server"
//#define MAINNET_ARMORY_BLOCKSETTLE_KEY "03a8649b32b9459961e143c5c111b9a47ffa494116791c1cb35945a8b9bc8254ab"
#define MAINNET_ARMORY_BLOCKSETTLE_ADDRESS "armory.blocksettle.com"
#define MAINNET_ARMORY_BLOCKSETTLE_PORT 80

// The class is used as a callback that processes asynchronous Armory events.
class ArmoryCallback : public RemoteCallback
{
public:
   ArmoryCallback(ArmoryConnection *conn, const std::shared_ptr<spdlog::logger> &logger)
      : RemoteCallback(), connection_(conn), logger_(logger) {}
   virtual ~ArmoryCallback(void) noexcept = default;

   void run(BDMAction action, void* ptr, int block = 0) override;
   void progress(BDMPhase phase,
      const std::vector<std::string> &walletIdVec,
      float progress, unsigned secondsRem,
      unsigned progressNumeric) override;

   void disconnected() override;

private:
   ArmoryConnection * connection_;
   std::shared_ptr<spdlog::logger>  logger_;
};

namespace bs {
   struct TXEntry {
      BinaryData  txHash;
      std::string id;
      int64_t     value;
      uint32_t    blockNum;
      uint32_t    txTime;
      bool        isRBF;
      bool        isChainedZC;

      static TXEntry fromLedgerEntry(const ClientClasses::LedgerEntry &);
      static std::vector<TXEntry> fromLedgerEntries(std::vector<ClientClasses::LedgerEntry>);
   };
}

// The abstracted connection between BS and Armory. When BS code needs to
// communicate with Armory, this class is what the code should use. Only one
// connection should exist at any given time.
class ArmoryConnection : public QObject
{
   friend class ArmoryCallback;
   Q_OBJECT
public:
   enum class State : uint8_t {
      Offline,
      Connecting,
      Canceled,
      Connected,
      Scanning,
      Error,
      Closing,
      Ready
   };

   ArmoryConnection(const std::shared_ptr<spdlog::logger> &, const std::string &txCacheFN
      , bool cbInMainThread = false);
   ~ArmoryConnection() noexcept;

   State state() const { return state_; }

   void setupConnection(const ArmorySettings &
                        , std::function<bool (const BinaryData&, const std::string&)> bip150PromptUserRoutine
                        = [](const BinaryData&, const std::string&){return true;});
   bool goOnline();

   bool broadcastZC(const BinaryData& rawTx);

   unsigned int topBlock() const { return topBlock_; }

   std::string registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &, const std::string &walletId
      , const std::vector<BinaryData> &addrVec, std::function<void(const std::string &)>, bool asNew = false);
   bool getWalletsHistory(const std::vector<std::string> &walletIDs
      , std::function<void (std::vector<ClientClasses::LedgerEntry>)>);

   // If context is not null and cbInMainThread is true then the callback will be called
   // on main thread only if context is still alive.
   bool getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &
      , std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>, QObject *context = nullptr);
   bool getWalletsLedgerDelegate(std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>);

   bool getTxByHash(const BinaryData &hash, std::function<void(Tx)>);
   bool getTXsByHash(const std::set<BinaryData> &hashes, std::function<void(std::vector<Tx>)>);
   bool getRawHeaderForTxHash(const BinaryData& inHash,
                              std::function<void(BinaryData)> callback);
   bool getHeaderByHeight(const unsigned& inHeight,
                          std::function<void(BinaryData)> callback);

   bool estimateFee(unsigned int nbBlocks, std::function<void(float)>);
   bool getFeeSchedule(std::function<void(std::map<unsigned int, float>)> cb);

   bool isTransactionVerified(const ClientClasses::LedgerEntry &) const;
   bool isTransactionVerified(uint32_t blockNum) const;
   bool isTransactionConfirmed(const ClientClasses::LedgerEntry &) const;
   unsigned int getConfirmationsNumber(const ClientClasses::LedgerEntry &item) const;
   unsigned int getConfirmationsNumber(uint32_t blockNum) const;

   bool isOnline() const { return isOnline_; }

   auto bip150PromptUser(const BinaryData& srvPubKey
                         , const std::string& srvIPPort) -> bool;

   void setState(State);
   std::atomic_bool  needsBreakConnectionLoop_ {false};
signals:
   void stateChanged(ArmoryConnection::State) const;
   void connectionError(QString) const;
   void prepareConnection(ArmorySettings server) const;
   void progress(BDMPhase, float progress, unsigned int secondsRem, unsigned int numProgress) const;
   void newBlock(unsigned int height) const;
   void zeroConfReceived(const std::vector<bs::TXEntry>) const;
   void zeroConfInvalidated(const std::vector<bs::TXEntry>) const;
   void refresh(std::vector<BinaryData> ids, bool online) const;
   void nodeStatus(NodeStatus, bool segWitEnabled, RpcStatus) const;
   void txBroadcastError(QString txHash, QString error) const;
   void error(QString errorStr, QString extraMsg) const;

private:
   void registerBDV(NetworkType);
   void setTopBlock(unsigned int topBlock) { topBlock_ = topBlock; }
   void onRefresh(std::vector<BinaryData>);
   void onZCsReceived(const std::vector<ClientClasses::LedgerEntry> &);
   void onZCsInvalidated(const std::set<BinaryData> &);

   void stopServiceThreads();
   bool startLocalArmoryProcess(const ArmorySettings &settings);

   bool addGetTxCallback(const BinaryData &hash, const std::function<void(Tx)> &);  // returns true if hash exists
   void callGetTxCallbacks(const BinaryData &hash, const Tx &);

   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<AsyncClient::BlockDataViewer>   bdv_;
   std::shared_ptr<ArmoryCallback>  cbRemote_;
   std::atomic<State>   state_ = { State::Offline };
   std::atomic_uint     topBlock_ = { 0 };
   TxCacheFile    txCache_;
   const bool     cbInMainThread_;
   std::shared_ptr<BlockHeader> getTxBlockHeader_;

   std::vector<SecureBinaryData> bsBIP150PubKeys_;

   std::atomic_bool  regThreadRunning_;
   std::atomic_bool  connThreadRunning_;
   std::atomic_bool  maintThreadRunning_;

   std::shared_ptr<QProcess>  armoryProcess_;

   std::atomic_bool              isOnline_;
   std::unordered_map<std::string, std::function<void(const std::string &)>>  preOnlineRegIds_;

   mutable std::atomic_flag      txCbLock_ = ATOMIC_FLAG_INIT;
   std::map<BinaryData, std::vector<std::function<void(Tx)>>>   txCallbacks_;

   std::map<BinaryData, bs::TXEntry>   zcEntries_;
};

#endif // __ARMORY_CONNECTION_H__
