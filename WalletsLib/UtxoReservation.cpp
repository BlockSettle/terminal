/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UtxoReservation.h"
#include <thread>
#include <spdlog/spdlog.h>
#include "FastLock.h"

using namespace bs;


// Unreserve all UTXOs for a given reservation ID. Associated wallet ID is the
// return value. Return the associated wallet ID. Adapter acts as a frontend for
// the actual reservation class.
std::string bs::UtxoReservation::Adapter::unreserve(const std::string &reserveId)
{
   if (!parent_) {
      return {};
   }
   return parent_->unreserve(reserveId);
}

// Get UTXOs for a given reservation ID. Adapter acts as a frontend for the
// actual reservation class.
std::vector<UTXO> bs::UtxoReservation::Adapter::get(const std::string &reserveId) const
{
   if (!parent_) {
      return {};
   }
   return parent_->get(reserveId);
}

// Reserve UTXOs for given wallet and reservation IDs. True if success, false if
// failure. Adapter acts as a frontend for the actual reservation class.
bool bs::UtxoReservation::Adapter::reserve(const std::string &walletId
                                           , const std::string &reserveId
                                           , const std::vector<UTXO> &utxos)
{
   if (!parent_) {
      return false;
   }
   parent_->reserve(walletId, reserveId, utxos);
   return true;
}

// For a given wallet ID, filter out all associated UTXOs from a list of UTXOs.
// True if success, false if failure. Adapter acts as a frontend for the actual
// reservation class.
bool bs::UtxoReservation::Adapter::filter(const std::string &walletId
                                          , std::vector<UTXO> &utxos) const
{
   if (!parent_) {
      return false;
   }
   return parent_->filter(walletId, utxos);
}

// Global UTXO reservation singleton.
static std::shared_ptr<bs::UtxoReservation> utxoResInstance_;

bs::UtxoReservation::UtxoReservation()
{
   if (!utxoResInstance_) {
      return;
   }

   std::thread([this] {
      const std::chrono::duration<int> secsToExpire(600);
      std::this_thread::sleep_for(std::chrono::seconds(10));
      const auto curTime = std::chrono::system_clock::now();
      std::vector<std::string> expiredResId;
      {
         FastLock lock(flag_);
         for (const auto &resIdTime : reserveTime_) {
            if ((curTime - resIdTime.second) > secsToExpire) {
               expiredResId.push_back(resIdTime.first);
            }
         }
      }
      for (const auto &resId : expiredResId) {
         unreserve(resId);
      }
   }).detach();
}

// Singleton reservation.
void bs::UtxoReservation::init(const std::shared_ptr<spdlog::logger> &logger)
{
   assert(!utxoResInstance_);
   utxoResInstance_ = std::make_shared<bs::UtxoReservation>();
   utxoResInstance_->logger_ = logger;
}

// Add an adapter to the singleton. True if success, false if failure.
bool bs::UtxoReservation::addAdapter(const std::shared_ptr<Adapter> &a)
{
   if (!utxoResInstance_) {
      return false;
   }
   a->setParent(utxoResInstance_.get());
   FastLock lock(utxoResInstance_->flag_);
   utxoResInstance_->adapters_.push_back(a);
   return true;
}

// Remove an adapter from the singleton. True if success, false if failure.
bool bs::UtxoReservation::delAdapter(const std::shared_ptr<Adapter> &a)
{
   if (!utxoResInstance_ || !a) {
      return false;
   }
   FastLock lock(utxoResInstance_->flag_);
   const auto pos = std::find(utxoResInstance_->adapters_.begin()
                              , utxoResInstance_->adapters_.end(), a);
   if (pos == utxoResInstance_->adapters_.end()) {
      return false;
   }
   a->setParent(nullptr);
   utxoResInstance_->adapters_.erase(pos);
   return true;
}

// Reserve a set of UTXOs for a wallet and reservation ID. Reserve across all
// active adapters.
void bs::UtxoReservation::reserve(const std::string &walletId
                                  , const std::string &reserveId
                                  , const std::vector<UTXO> &utxos)
{
   const auto curTime = std::chrono::system_clock::now();
   FastLock lock(flag_);

   auto it = byReserveId_.find(reserveId);
   if (it != byReserveId_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "reservation '{}' already exist", reserveId);
      return;
   }

   byReserveId_[reserveId] = utxos;
   walletByReserveId_[reserveId] = walletId;
   resIdByWalletId_[walletId].insert(reserveId);
   reserveTime_[reserveId] = curTime;
   for (const auto &a : adapters_) {
      a->reserved(walletId, utxos);
   }
}

// Unreserve a set of UTXOs for a wallet and reservation ID. Return the
// associated wallet ID. Unreserve across all active adapters.
std::string bs::UtxoReservation::unreserve(const std::string &reserveId)
{
   FastLock lock(flag_);
   const auto it = byReserveId_.find(reserveId);
   if (it == byReserveId_.end()) {
      return {};
   }
   const auto &itWallet = walletByReserveId_.find(reserveId);
   if (itWallet == walletByReserveId_.end()) {
      return {};
   }
   const auto walletId = itWallet->second;

   byReserveId_.erase(reserveId);
   walletByReserveId_.erase(reserveId);
   reserveTime_.erase(reserveId);
   resIdByWalletId_[walletId].erase(reserveId);
   for (const auto &a : adapters_) {
      a->unreserved(walletId, reserveId);
   }
   return walletId;
}

// Get UTXOs for a given reservation ID.
std::vector<UTXO> bs::UtxoReservation::get(const std::string &reserveId) const
{
   FastLock lock(flag_);
   const auto it = byReserveId_.find(reserveId);
   if (it == byReserveId_.end()) {
      return {};
   }
   return it->second;
}

// For a given wallet ID, filter out all associated UTXOs from a list of UTXOs.
// True if success, false if failure.
bool bs::UtxoReservation::filter(const std::string &walletId
                                 , std::vector<UTXO> &utxos) const
{
   struct UtxoHasher {
      std::size_t operator()(const UTXO &utxo) const {
         return std::hash<std::string>()(utxo.getTxHash().toBinStr());
      }
   };
   std::unordered_set<UTXO, UtxoHasher> reserved;
   {
      FastLock lock(flag_);
      const auto &itResId = resIdByWalletId_.find(walletId);
      if (itResId == resIdByWalletId_.end()) {
         return false;
      }
      for (const auto &id : itResId->second) {
         const auto &itUtxos = byReserveId_.find(id);
         if (itUtxos != byReserveId_.end()) {
            reserved.insert(itUtxos->second.begin(), itUtxos->second.end());
         }
      }
   }
   if (reserved.empty()) {
      return false;
   }

   for (const auto &utxo : reserved) {
      const auto pos = std::find(utxos.begin(), utxos.end(), utxo);
      if (pos != utxos.end()) {
         utxos.erase(pos);
      }
   }
   return true;
}

UtxoReservation *UtxoReservation::instance()
{
   return utxoResInstance_.get();
}
