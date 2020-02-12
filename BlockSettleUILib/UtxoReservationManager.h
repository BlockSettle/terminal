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

   class UTXOReservationManager : public QObject
   {
      Q_OBJECT
   public:
      UTXOReservationManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager_,
         const std::shared_ptr<spdlog::logger>& logger_, QObject* parent = nullptr);
      ~UTXOReservationManager() override;

      UTXOReservationManager(const UTXOReservationManager &) = delete;
      UTXOReservationManager &operator=(const UTXOReservationManager &) = delete;

      UTXOReservationManager(UTXOReservationManager &&) = delete;
      UTXOReservationManager &operator=(UTXOReservationManager &&) = delete;

      FixedXbtInputs reserveBestUtxoSet(const std::string& walletId,
         const std::shared_ptr<bs::network::RFQ>& rfq, BTCNumericTypes::balance_type offer);
      
      uint64_t getAvailableUtxoSum(const std::string& walletId) const;
      std::vector<UTXO> getAvailableUTXOs(const std::string& walletId) const;

      UtxoReservationToken makeNewReservation(const std::vector<UTXO> &utxos, const std::string &reserveId);
   
   signals:
      void availableUtxoChanged(const std::string& walledId);
      //void reserveBestUtxoSet();

   private slots:
      void refreshAvailableUTXO();
      void onWalletsDeleted(const std::string& walledId);
      void onWalletsAdded(const std::string& walledId);
      void onWalletsBalanceChanged(const std::string& walledId);

   private:
      std::map<std::string, std::vector<UTXO>> availableUTXOs_;

      std::shared_ptr<spdlog::logger> logger_;
      std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   };

}  // namespace bs

#endif // UTXO_RESERVATION_MANAGER_H
