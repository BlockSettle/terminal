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

   bool getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb &) override;

   bool getWalletsLedgerDelegate(const LedgerDelegateCb &) override;

   bool getTxByHash(const BinaryData &hash, const TxCb &) override;
   bool getTXsByHash(const std::set<BinaryData> &hashes, const TXsCb &) override;
   bool getRawHeaderForTxHash(const BinaryData& inHash, const BinaryDataCb &) override;
   bool getHeaderByHeight(const unsigned int inHeight, const BinaryDataCb &) override;

   bool estimateFee(unsigned int nbBlocks, const FloatCb &) override;
   bool getFeeSchedule(const FloatMapCb &) override;

private:
   bool startLocalArmoryProcess(const ArmorySettings &);

   bool needInvokeCb() const;

private:
   const bool     cbInMainThread_;
#ifdef USE_LOCAL_TX_CACHE
   TxCacheFile    txCache_;
#endif
   std::shared_ptr<QProcess>  armoryProcess_;
};

#endif // ARMORY_OBJECT_H
