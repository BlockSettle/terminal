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
class ArmoryObject;

namespace bs {

   class UTXOReservationManager : public QObject
   {
      Q_OBJECT
   public:
      UTXOReservationManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager, const std::shared_ptr<ArmoryObject>& armory,
         const std::shared_ptr<spdlog::logger>& logger, QObject* parent = nullptr);
      ~UTXOReservationManager() override;

      UTXOReservationManager(const UTXOReservationManager &) = delete;
      UTXOReservationManager &operator=(const UTXOReservationManager &) = delete;

      UTXOReservationManager(UTXOReservationManager &&) = delete;
      UTXOReservationManager &operator=(UTXOReservationManager &&) = delete;

      void reserveBestUtxoSet(const std::string& walletId, BTCNumericTypes::satoshi_type quantity,
         std::function<void(FixedXbtInputs&&)>&& cb);
      
      BTCNumericTypes::satoshi_type getAvailableUtxoSum(const std::string& walletId) const;
      std::vector<UTXO> getAvailableUTXOs(const std::string& walletId) const;

      UtxoReservationToken makeNewReservation(const std::vector<UTXO> &utxos, const std::string &reserveId);
      UtxoReservationToken makeNewReservation(const std::vector<UTXO> &utxos);

      void getBestUtxoSet(const std::string& walletId, BTCNumericTypes::satoshi_type quantity,
         std::function<void(std::vector<UTXO>&&)>&& cb);

      static FixedXbtInputs convertUtxoToFixedInput(const std::string& walletId, const std::vector<UTXO>& utxos);
   
   signals:
      void availableUtxoChanged(const std::string& walledId);

   private slots:
      void refreshAvailableUTXO();
      void onWalletsDeleted(const std::string& walledId);
      void onWalletsAdded(const std::string& walledId);
      void onWalletsBalanceChanged(const std::string& walledId);

   private:
      std::map<std::string, std::vector<UTXO>> availableUTXOs_;

      std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
      std::shared_ptr<ArmoryObject> armory_;
      std::shared_ptr<spdlog::logger> logger_;
   };

}  // namespace bs

#endif // UTXO_RESERVATION_MANAGER_H
