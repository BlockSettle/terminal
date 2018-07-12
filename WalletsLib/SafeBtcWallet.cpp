#include "SafeBtcWallet.h"
#include "FastLock.h"
#include "PyBlockDataManager.h"
#include <QDebug>

SafeBtcWallet::SafeBtcWallet(const SwigClient::BtcWallet& btcWallet, std::atomic_flag &lock)
   : btcWallet_(btcWallet)
   , bdvLock_(lock)
{}

std::vector<uint64_t> SafeBtcWallet::getBalancesAndCount(uint32_t topBlockHeight)
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getBalancesAndCount(topBlockHeight);
   } catch (const std::exception &e) {
      qDebug() << "[SafeBtcWallet::getBalancesAndCount] exception:" << e.what();
   }
   catch (...) {
      qDebug() << "[SafeBtcWallet::getBalancesAndCount] exception";
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::vector<uint64_t>{};
}

std::map<BinaryData, uint32_t> SafeBtcWallet::getAddrTxnCountsFromDB()
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getAddrTxnCountsFromDB();
   } catch (...) {
      qDebug() << QLatin1String("[SafeBtcWallet::getAddrTxnCountsFromDB] exception");
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::map<BinaryData, uint32_t>{};
}

std::map<BinaryData, std::vector<uint64_t>> SafeBtcWallet::getAddrBalancesFromDB()
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getAddrBalancesFromDB();
   } catch (...) {
      qDebug() << QLatin1String("[SafeBtcWallet::getAddrBalancesFromDB] exception");
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::map<BinaryData, std::vector<uint64_t>>{};
}

std::vector<UTXO> SafeBtcWallet::getSpendableTxOutListForValue(uint64_t val)
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getSpendableTxOutListForValue(val);
   } catch (const std::exception &e) {
      qDebug() << QLatin1String("[SafeBtcWallet::getSpendableTxOutListForValue] exception:") << e.what();
      return {};
   } catch (...) {
      qDebug() << QLatin1String("[SafeBtcWallet::getSpendableTxOutListForValue] exception");
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::vector<UTXO>{};
}

std::vector<UTXO> SafeBtcWallet::getSpendableZCList()
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getSpendableZCList();
   } catch (...) {
      qDebug() << QLatin1String("[SafeBtcWallet::getSpendableZCList] exception");
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::vector<UTXO>{};
}

std::vector<UTXO> SafeBtcWallet::getRBFTxOutList()
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getRBFTxOutList();
   } catch (...) {
      qDebug() << QLatin1String("[SafeBtcWallet::getRBFTxOutList] exception");
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::vector<UTXO>{};
}

std::vector<ClientClasses::LedgerEntry> SafeBtcWallet::getHistoryPage(uint32_t id)
{
   try {
      FastLock lock(bdvLock_);
      return btcWallet_.getHistoryPage(id);
   }
   catch (const std::exception &e) {
      qDebug() << QLatin1String("[SafeBtcWallet::getHistoryPage] exception: ") << e.what();
      return {};
   } catch (...) {
      qDebug() << QLatin1String("[SafeBtcWallet::getHistoryPage] exception");
   }
   PyBlockDataManager::instance()->OnConnectionError();
   return std::vector<ClientClasses::LedgerEntry>{};
}
