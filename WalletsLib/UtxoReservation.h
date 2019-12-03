/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __UTXO_RESERVATION_H__
#define __UTXO_RESERVATION_H__

#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include "TxClasses.h"

namespace spdlog {
   class logger;
}

namespace bs {
   // A reservation system for UTXOs. It can be fed a list of inputs. The inputs
   // are then set aside and made unavailable for future usage. This is useful
   // for keeping UTXOs from being used, and for accessing UTXOs later (e.g.,
   // when zero conf TXs arrive and the inputs need to be accessed quickly).
   //
   // NB: This is a global singleton that shouldn't be accessed directly. Use an
   // Adapter object to access the singleton and do all the heavy lifting.
   class UtxoReservation
   {
   public:
      // Adapters can be used directly or derived and altered as needed.
      // Adapters make of reservation IDs (any unique string), wallet IDs (which
      // can have multiple associated reservation IDs), and a set of reserved
      // UTXOs.
      class Adapter {
         friend class UtxoReservation;
      public:
         virtual ~Adapter() noexcept = default;
         bool reserve(const std::string &walletId, const std::string &reserveId
                      , const std::vector<UTXO> &);
         std::string unreserve(const std::string &reserveId);
         std::vector<UTXO> get(const std::string &reserveId) const;
         bool filter(const std::string &walletId
                     , std::vector<UTXO> &utxos) const;
      private:
         void setParent(UtxoReservation *parent) { parent_ = parent; }
         virtual void reserved(const std::string &walletID
                               , const std::vector<UTXO> &resUTXOs) {}
         virtual void unreserved(const std::string &walletID
                                 , const std::string &reserveID) {}
      protected:
         UtxoReservation * parent_ = nullptr;
      };

      explicit UtxoReservation();

      // Create the singleton. Use only once!
      // Destroying disabled as it's broken, see BST-2362 for details
      static void init(const std::shared_ptr<spdlog::logger> &logger);

      // Add and remove individual adapters. Typically added/deleted only once
      // per class that uses an adapter.
      static bool addAdapter(const std::shared_ptr<Adapter> &a);
      static bool delAdapter(const std::shared_ptr<Adapter> &a);

      // Reserve/Unreserve UTXOs. Used as needed. User supplies the wallet ID,
      // a reservation ID, and the UTXOs to reserve.
      void reserve(const std::string &walletId, const std::string &reserveId
                   , const std::vector<UTXO> &utxos);
      std::string unreserve(const std::string &reserveId);  // returns walletId

      // Get the UTXOs based on the reservation ID.
      std::vector<UTXO> get(const std::string &reserveId) const;

      // Pass in a vector of UTXOs. If any of the UTXOs are in the wallet ID
      // being queried, remove the UTXOs from the vector.
      bool filter(const std::string &walletId, std::vector<UTXO> &utxos) const;

      static UtxoReservation *instance();

   private:
      using UTXOs = std::vector<UTXO>;
      using IdList = std::unordered_set<std::string>;

      mutable std::atomic_flag                     flag_ = ATOMIC_FLAG_INIT;
      // Reservation ID, UTXO vector.
      std::unordered_map<std::string, UTXOs>       byReserveId_;
      // Reservation ID, WalletID
      std::unordered_map<std::string, std::string> walletByReserveId_;
      // Wallet ID, unordered set of Reservation IDs
      std::unordered_map<std::string, IdList>      resIdByWalletId_;
      // Reservation ID, time of reservation
      std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock>> reserveTime_;
      // Active adapters.
      std::vector<std::shared_ptr<Adapter>>        adapters_;

      std::shared_ptr<spdlog::logger> logger_;
   };

}  //namespace bs

#endif //__UTXO_RESERVATION_H__
