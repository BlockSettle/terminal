/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef UTXO_RESERVATION_MANAGER_H
#define UTXO_RESERVATION_MANAGER_H

#include <QObject>
#include "CommonTypes.h"
#include "UtxoReservationToken.h"

namespace spdlog {
   class logger;
}

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

namespace bs {

   struct FixedXbtInputs
   {
      // Need to use UTXO/walletId map for CC
      std::map<UTXO, std::string> inputs;
      bs::UtxoReservationToken utxoRes;
   };

   class UTXOReservantionManager : public QObject
   {
      Q_OBJECT
      friend class UtxoReservationToken;
   public:
      UTXOReservantionManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager_,
         const std::shared_ptr<spdlog::logger>& logger_, QObject* parent = nullptr);
      ~UTXOReservantionManager() override = default;

      UTXOReservantionManager(const UTXOReservantionManager &) = delete;
      UTXOReservantionManager &operator=(const UTXOReservantionManager &) = delete;

      UTXOReservantionManager(UTXOReservantionManager &&) = delete;
      UTXOReservantionManager &operator=(UTXOReservantionManager &&) = delete;

      FixedXbtInputs reserveBestUtxoSet(const std::string& walletId,
         const std::shared_ptr<bs::network::RFQ>& rfq, BTCNumericTypes::balance_type offer,
         const std::shared_ptr<UTXOReservantionManager>& reservationMgr);
      
      uint64_t getAvailableUtxoSum(const std::string& walletid) const;
   
   signals:
      void availableUtxoChanged(const std::string& walledId);

   private slots:
      void refreshAvailableUTXO();

   private:
      std::map<std::string, std::vector<UTXO>> availableUTXOs_;

      std::shared_ptr<spdlog::logger> logger_;
      std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   };

}  // namespace bs

#endif // UTXO_RESERVATION_MANAGER_H
