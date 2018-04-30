#ifndef __SAFE_BTC_WALLET_H__
#define __SAFE_BTC_WALLET_H__

#include <memory>
#include <vector>
#include <map>
#include <QMutex>
#include "BinaryData.h"
#include "LedgerEntryData.h"
#include "SwigClient.h"
#include "TxClasses.h"

class SafeBtcWallet
{
public:
   SafeBtcWallet(const SwigClient::BtcWallet &btcWallet, std::atomic_flag &lock);
   ~SafeBtcWallet() noexcept = default;

   SafeBtcWallet(const SafeBtcWallet&) = delete;
   SafeBtcWallet& operator = (const SafeBtcWallet&) = delete;

   SafeBtcWallet(SafeBtcWallet&&) = delete;
   SafeBtcWallet& operator = (SafeBtcWallet&&) = delete;

   std::vector<uint64_t> getBalancesAndCount(uint32_t topBlockHeight, bool IGNOREZC);
   std::map<BinaryData, uint32_t> getAddrTxnCountsFromDB(void);

   std::map<BinaryData, std::vector<uint64_t> > getAddrBalancesFromDB(void);

   std::vector<UTXO> getSpendableTxOutListForValue(uint64_t val = UINT64_MAX);
   std::vector<UTXO> getSpendableZCList();
   std::vector<UTXO> getRBFTxOutList();

   std::vector<LedgerEntryData> getHistoryPage(uint32_t id);

private:
   SwigClient::BtcWallet   btcWallet_;
   std::atomic_flag     &  bdvLock_;
};

#endif // __SAFE_BTC_WALLET_H__