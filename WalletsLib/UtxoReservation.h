#ifndef __UTXO_RESERVATION_H__
#define __UTXO_RESERVATION_H__

#include <atomic>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include "TxClasses.h"


namespace bs {

   class UtxoReservation
   {
   public:
      class Adapter {
         friend class UtxoReservation;
      public:
         bool reserve(const std::string &walletId, const std::string &reserveId, const std::vector<UTXO> &);
         std::string unreserve(const std::string &reserveId);
         std::vector<UTXO> get(const std::string &reserveId) const;
         bool filter(const std::string &walletId, std::vector<UTXO> &) const;
      private:
         void setParent(UtxoReservation *parent) { parent_ = parent; }
         virtual void reserved(const std::string &, const std::vector<UTXO> &) {}
         virtual void unreserved(const std::string &, const std::string &reserveId) {}
      protected:
         UtxoReservation * parent_ = nullptr;
      };

      static void init();
      static void destroy();
      static bool addAdapter(const std::shared_ptr<Adapter> &a);
      static bool delAdapter(const std::shared_ptr<Adapter> &a);

      void reserve(const std::string &walletId, const std::string &reserveId, const std::vector<UTXO> &);
      std::string unreserve(const std::string &reserveId);  // returns walletId
      std::vector<UTXO> get(const std::string &reserveId) const;
      bool filter(const std::string &walletId, std::vector<UTXO> &) const;

   private:
      using UTXOs = std::vector<UTXO>;
      using IdList = std::unordered_set<std::string>;

      mutable std::atomic_flag                     flag_ = ATOMIC_FLAG_INIT;
      std::unordered_map<std::string, UTXOs>       byReserveId_;
      std::unordered_map<std::string, std::string> walletByReserveId_;
      std::unordered_map<std::string, IdList>      resIdByWalletId_;
      std::vector<std::shared_ptr<Adapter>>        adapters_;
   };

}  //namespace bs

#endif //__UTXO_RESERVATION_H__
