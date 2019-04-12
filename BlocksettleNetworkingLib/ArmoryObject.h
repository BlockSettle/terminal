#ifndef ARMORY_OBJECT_H
#define ARMORY_OBJECT_H

#include <QProcess>
#include "ArmoryConnection.h"
#include "ArmorySettings.h"
#include "CacheFile.h"


class ArmoryObject : public QObject, public ArmoryConnection
{
   Q_OBJECT
public:
   ArmoryObject(const std::shared_ptr<spdlog::logger> &, const std::string &txCacheFN
      , bool cbInMainThread = true);
   ~ArmoryObject() noexcept = default;

   void setupConnection(const ArmorySettings &settings
      , const std::function<bool(const BinaryData &, const std::string &)> &bip150PromptUserCb
      = [](const BinaryData&, const std::string&) {return true; });

   std::string registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &, const std::string &walletId
      , const std::vector<BinaryData> &addrVec, const std::function<void(const std::string &)> &
      , bool asNew = false) override;
   bool getWalletsHistory(const std::vector<std::string> &walletIDs
      , const std::function<void(std::vector<ClientClasses::LedgerEntry>)> &) override;

   // If context is not null and cbInMainThread is true then the callback will be called
   // on main thread only if context is still alive.
   bool getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &
      , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &
      , QObject *context = nullptr);
   bool getWalletsLedgerDelegate(
      const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &) override;

   bool getTxByHash(const BinaryData &hash, const std::function<void(Tx)> &) override;
   bool getTXsByHash(const std::set<BinaryData> &hashes, const std::function<void(std::vector<Tx>)> &) override;
   bool getRawHeaderForTxHash(const BinaryData& inHash, const std::function<void(BinaryData)> &) override;
   bool getHeaderByHeight(const unsigned int inHeight, const std::function<void(BinaryData)> &) override;

   bool estimateFee(unsigned int nbBlocks, const std::function<void(float)> &) override;
   bool getFeeSchedule(const std::function<void(std::map<unsigned int, float>)> &) override;

   auto bip150PromptUser(const BinaryData& srvPubKey
      , const std::string& srvIPPort) -> bool;

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
   bool startLocalArmoryProcess(const ArmorySettings &);

private:
   const bool     cbInMainThread_;
   TxCacheFile    txCache_;
   std::shared_ptr<QProcess>  armoryProcess_;
};

#endif // ARMORY_OBJECT_H
