/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CC_LOGIC_ASYNC_H
#define CC_LOGIC_ASYNC_H

#include <vector>
#include <set>
#include <map>
#include <string>

#include "Address.h"
#include "ArmoryConnection.h"
#include "ColoredCoinLogic.h"


class ColoredCoinTrackerAsync
{
private:
   std::set<BinaryData> originAddresses_;
   std::set<BinaryData> revocationAddresses_;

   std::shared_ptr<ArmoryConnection> connPtr_;

   std::shared_ptr<ColoredCoinSnapshot> snapshot_ = nullptr;
   std::shared_ptr<ColoredCoinZCSnapshot> zcSnapshot_ = nullptr;

   unsigned startHeight_ = 0;
   unsigned zcCutOff_ = 0;
   
   uint64_t coinsPerShare_;

   std::atomic<bool> ready_;

protected:
   std::shared_ptr<AsyncClient::BtcWallet> walletObj_;

private:
   using ResultCb = std::function<void(bool)>;

   ParsedCcTx processTx(
      const std::shared_ptr<ColoredCoinSnapshot> &,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const Tx&) const;

   // executed recursively until there will be no hashes to process
   void processTxBatch(const std::shared_ptr<ColoredCoinSnapshot> &
      , const std::set<BinaryData>&, const ResultCb &);

   void processZcBatch(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const std::set<BinaryData>&, const ResultCb &);

   void processRevocationBatch(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::set<BinaryData>&, const ResultCb &);

   void purgeZc(const ResultCb &);

   void addUtxo(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const BinaryData& txHash, unsigned txOutIndex,
      uint64_t value, const BinaryData& scrAddr);

   void addZcUtxo(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const BinaryData& txHash, unsigned txOutIndex,
      uint64_t value, const BinaryData& scrAddr);

   const std::shared_ptr<BinaryData> getScrAddrPtr(
      const std::map<BinaryData, OpPtrSet>&,
      const BinaryData&) const;

   void eraseScrAddrOp(
      const std::shared_ptr<ColoredCoinSnapshot> &,
      const std::shared_ptr<CcOutpoint>&);
   
   void addScrAddrOp(
      std::map<BinaryData, OpPtrSet>&,
      const std::shared_ptr<CcOutpoint>&);

   uint64_t getCcOutputValue(
      const std::shared_ptr<ColoredCoinSnapshot> &
      , const std::shared_ptr<ColoredCoinZCSnapshot>&
      , const BinaryData&, unsigned, unsigned) const;

   std::shared_ptr<ColoredCoinSnapshot> snapshot(void) const;
   std::shared_ptr<ColoredCoinZCSnapshot> zcSnapshot(void) const;

   void shutdown(void);

protected:
   void reorg(bool hard);

   using AddrSetCb = std::function<void(const std::set<BinaryData> &)>;
   void update(const AddrSetCb &);
   void zcUpdate(const AddrSetCb &);

public:
   ColoredCoinTrackerAsync(uint64_t coinsPerShare,
      std::shared_ptr<ArmoryConnection> connPtr) :
      coinsPerShare_(coinsPerShare), connPtr_(connPtr)
   {
      ready_.store(false, std::memory_order_relaxed);

      auto&& wltIdSbd = CryptoPRNG::generateRandom(12);
      walletObj_ = connPtr_->instantiateWallet(wltIdSbd.toHexStr());
   }

   ~ColoredCoinTrackerAsync(void)
   {
      shutdown();
   }

   ////
   void addOriginAddress(const bs::Address&);
   void addRevocationAddress(const bs::Address&);

   // returned lambda should be invoked when returned regId is registered
   std::pair<std::string, std::function<void()>> goOnline(const std::function<void(bool)> &);

   using RefreshCb = std::function<void(const std::string &)>;
   void onZeroConf(const RefreshCb &);
   void onNewBlock(unsigned int branchHeight, const RefreshCb &);

   //in: hash, tx index, txout index
   uint64_t getCcOutputValue(const BinaryData&, unsigned, unsigned) const;

   //in: prefixed address
   uint64_t getCcValueForAddress(const BinaryData&) const;

   //in: prefixed address
   std::vector<std::shared_ptr<CcOutpoint>> getSpendableOutpointsForAddress(
      const BinaryData&) const;
};

#endif   //CC_LOGIC_ASYNC_H
