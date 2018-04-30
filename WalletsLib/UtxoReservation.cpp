#include "UtxoReservation.h"
#include "FastLock.h"

using namespace bs;


std::string bs::UtxoReservation::Adapter::unreserve(const std::string &id)
{
   if (!parent_) {
      return {};
   }
   return parent_->unreserve(id);
}

std::vector<UTXO> bs::UtxoReservation::Adapter::get(const std::string &id) const
{
   if (!parent_) {
      return {};
   }
   return parent_->get(id);
}

bool bs::UtxoReservation::Adapter::reserve(const std::string &walletId, const std::string &reserveId, const std::vector<UTXO> &utxos)
{
   if (!parent_) {
      return false;
   }
   parent_->reserve(walletId, reserveId, utxos);
   return true;
}

bool bs::UtxoReservation::Adapter::filter(const std::string &walletId, std::vector<UTXO> &utxos) const
{
   if (!parent_) {
      return false;
   }
   return parent_->filter(walletId, utxos);
}


static std::shared_ptr<bs::UtxoReservation> utxoResInstance_;

void bs::UtxoReservation::init()
{
   utxoResInstance_ = std::make_shared<bs::UtxoReservation>();
}

void bs::UtxoReservation::destroy()
{
   utxoResInstance_ = nullptr;
}

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

bool bs::UtxoReservation::delAdapter(const std::shared_ptr<Adapter> &a)
{
   if (!utxoResInstance_ || !a) {
      return false;
   }
   FastLock lock(utxoResInstance_->flag_);
   const auto pos = std::find(utxoResInstance_->adapters_.begin(), utxoResInstance_->adapters_.end(), a);
   if (pos == utxoResInstance_->adapters_.end()) {
      return false;
   }
   a->setParent(nullptr);
   utxoResInstance_->adapters_.erase(pos);
   return true;
}

void bs::UtxoReservation::reserve(const std::string &walletId, const std::string &reserveId, const std::vector<UTXO> &utxos)
{
   FastLock lock(flag_);
   byReserveId_[reserveId] = utxos;
   walletByReserveId_[reserveId] = walletId;
   resIdByWalletId_[walletId].insert(reserveId);
   for (const auto &a : adapters_) {
      a->reserved(walletId, utxos);
   }
}

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
   resIdByWalletId_[walletId].erase(reserveId);
   for (const auto &a : adapters_) {
      a->unreserved(walletId, reserveId);
   }
   return walletId;
}

std::vector<UTXO> bs::UtxoReservation::get(const std::string &reserveId) const
{
   FastLock lock(flag_);
   const auto it = byReserveId_.find(reserveId);
   if (it == byReserveId_.end()) {
      return {};
   }
   return it->second;
}

bool bs::UtxoReservation::filter(const std::string &walletId, std::vector<UTXO> &utxos) const
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
