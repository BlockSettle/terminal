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
   ~ArmoryObject() noexcept override = default;

   void setupConnection(const ArmorySettings &settings
      , const BIP151Cb &bip150PromptUserCb = [](const BinaryData&, const std::string&) { return true; });

   std::string registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &, const std::string &walletId
      , const std::vector<BinaryData> &addrVec, const RegisterWalletCb &
      , bool asNew = false) override;
   bool getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb &) override;

   // If context is not null and cbInMainThread is true then the callback will be called
   // on main thread only if context is still alive.
   bool getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &
      , const LedgerDelegateCb &, QObject *context = nullptr);
   bool getWalletsLedgerDelegate(const LedgerDelegateCb &) override;

   bool getTxByHash(const BinaryData &hash, const TxCb &) override;
   bool getTXsByHash(const std::set<BinaryData> &hashes, const TXsCb &) override;
   bool getRawHeaderForTxHash(const BinaryData& inHash, const BinaryDataCb &) override;
   bool getHeaderByHeight(const unsigned int inHeight, const BinaryDataCb &) override;

   bool estimateFee(unsigned int nbBlocks, const FloatCb &) override;
   bool getFeeSchedule(const FloatMapCb &) override;

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
