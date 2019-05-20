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

#include <spdlog/logger.h>

#include "Address.h"
#include "AsyncClient.h"
#include "BtcDefinitions.h"
#include "BlockObj.h"

class ArmoryConnection;

// The class is used as a callback that processes asynchronous Armory events.
class ArmoryCallback : public RemoteCallback
{
public:
   ArmoryCallback(ArmoryConnection *conn, const std::shared_ptr<spdlog::logger> &logger)
      : RemoteCallback(), connection_(conn), logger_(logger) {}
   virtual ~ArmoryCallback() noexcept override = default;

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
class ArmoryConnection
{
   friend class ArmoryCallback;
public:
   enum class State : uint8_t {
      Offline,
      Connecting,
      Cancelled,
      Connected,
      Scanning,
      Error,
      Closing,
      Ready
   };

   ArmoryConnection(const std::shared_ptr<spdlog::logger> &);
   virtual ~ArmoryConnection() noexcept;

   State state() const { return state_; }

   bool goOnline();

   bool broadcastZC(const BinaryData& rawTx);

   unsigned int topBlock() const { return topBlock_; }

   using RegisterWalletCb = std::function<void(const std::string &regId)>;
   using WalletsHistoryCb = std::function<void (const std::vector<ClientClasses::LedgerEntry>&)>;
   using LedgerDelegateCb = std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>;

   virtual std::string registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &, const std::string &walletId
      , const std::vector<BinaryData> &addrVec, const RegisterWalletCb&
      , bool asNew = false);
   virtual bool getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb&);

   // If context is not null and cbInMainThread is true then the callback will be called
   // on main thread only if context is still alive.
   bool getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &, const LedgerDelegateCb &);
   virtual bool getWalletsLedgerDelegate(const LedgerDelegateCb &);

   using TxCb = std::function<void(const Tx&)>;
   using TXsCb = std::function<void(const std::vector<Tx>&)>;

   using BinaryDataCb = std::function<void(const BinaryData&)>;

   virtual bool getTxByHash(const BinaryData &hash, const TxCb&);
   virtual bool getTXsByHash(const std::set<BinaryData> &hashes, const TXsCb &);
   virtual bool getRawHeaderForTxHash(const BinaryData& inHash, const BinaryDataCb &);
   virtual bool getHeaderByHeight(const unsigned int inHeight, const BinaryDataCb &);

   using FloatCb = std::function<void(float)>;
   using FloatMapCb = std::function<void(const std::map<unsigned int, float> &)>;

   virtual bool estimateFee(unsigned int nbBlocks, const FloatCb &);
   virtual bool getFeeSchedule(const FloatMapCb&);

   bool isTransactionVerified(const ClientClasses::LedgerEntry &) const;
   bool isTransactionVerified(uint32_t blockNum) const;
   bool isTransactionConfirmed(const ClientClasses::LedgerEntry &) const;
   unsigned int getConfirmationsNumber(const ClientClasses::LedgerEntry &item) const;
   unsigned int getConfirmationsNumber(uint32_t blockNum) const;

   bool isOnline() const { return isOnline_; }

   void setState(State);
   std::atomic_bool  needsBreakConnectionLoop_ {false};

   using RefreshCb = std::function<void(const std::vector<BinaryData>&, bool)>;

   unsigned int setRefreshCb(const RefreshCb &);
   bool unsetRefreshCb(unsigned int);

   using StringCb = std::function<void(const std::string &)>;
   using BIP151Cb = std::function<bool(const BinaryData&, const std::string&)>;

protected:
   void setupConnection(NetworkType, const std::string &host, const std::string &port
      , const std::string &dataDir, const BinaryData &serverKey
      , const StringCb &cbError, const BIP151Cb &cbBIP151);

private:
   void registerBDV(NetworkType);
   void setTopBlock(unsigned int topBlock) { topBlock_ = topBlock; }
   void onRefresh(const std::vector<BinaryData> &);
   void onZCsReceived(const std::vector<ClientClasses::LedgerEntry> &);
   void onZCsInvalidated(const std::set<BinaryData> &);

   void stopServiceThreads();

   bool addGetTxCallback(const BinaryData &hash, const TxCb &);  // returns true if hash exists
   void callGetTxCallbacks(const BinaryData &hash, const Tx &);

protected:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<AsyncClient::BlockDataViewer>   bdv_;
   std::shared_ptr<ArmoryCallback>  cbRemote_;
   std::atomic<State>   state_ = { State::Offline };
   std::atomic_uint     topBlock_ = { 0 };
   std::shared_ptr<BlockHeader> getTxBlockHeader_;

   std::vector<SecureBinaryData> bsBIP150PubKeys_;

   std::atomic_bool  regThreadRunning_;
   std::atomic_bool  connThreadRunning_;
   std::atomic_bool  maintThreadRunning_;

   std::atomic_bool              isOnline_;
   std::unordered_map<std::string, RegisterWalletCb>  preOnlineRegIds_;

   mutable std::atomic_flag      txCbLock_ = ATOMIC_FLAG_INIT;
   std::map<BinaryData, std::vector<TxCb>>   txCallbacks_;

   std::map<BinaryData, bs::TXEntry>   zcEntries_;

   unsigned int cbSeqNo_ = 1;
   std::function<void(State)>    cbStateChanged_ = nullptr;
   std::map<unsigned int, RefreshCb> cbRefresh_;
   std::function<void(unsigned int)>                  cbNewBlock_ = nullptr;
   std::function<void(const std::vector<bs::TXEntry> &)>      cbZCReceived_ = nullptr;
   std::function<void(const std::vector<bs::TXEntry> &)>      cbZCInvalidated_ = nullptr;
   std::function<void(BDMPhase, float, unsigned int, unsigned int)>  cbProgress_ = nullptr;
   std::function<void(NodeStatus, bool, RpcStatus)>   cbNodeStatus_ = nullptr;
   std::function<void(const std::string &, const std::string &)>  cbError_ = nullptr;
   std::function<void(const std::string &, const std::string &)>  cbTxBcError_ = nullptr;
};

#endif // __ARMORY_CONNECTION_H__
