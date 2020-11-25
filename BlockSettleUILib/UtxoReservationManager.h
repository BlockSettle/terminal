/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef UTXO_RESERVATION_MANAGER_H
#define UTXO_RESERVATION_MANAGER_H

#include <atomic>
#include <QObject>
#include "CommonTypes.h"
#include "UiUtils.h"
#include "UtxoReservationToken.h"

namespace spdlog {
   class logger;
}

namespace bs {
   namespace sync {
      class WalletsManager;
      class Wallet;
      namespace hd {
         class Wallet;
      }
   }
}
class ArmoryObject;

namespace bs {

   class UTXOReservationManager : public QObject
   {
      Q_OBJECT
   public:
      using HDWalletId = std::string;
      using CCWalletId = std::string;
      using CCProductName = std::string;

      enum class CheckAmount {
         Enabled,
         Disabled,
      };

      enum class UtxoType {
         Normal,
         Zc,
      };

      using UtxoItemMap = std::map<UTXO, std::pair<std::string, UtxoType>>;

      const static bool kIncludeZcRequestor = false;
      const static bool kIncludeZcDealerSpotXbt = true;
      const static bool kIncludeZcDealerCc = false;
      const static bool kIncludeZcOtc = false;
      const static bool kIncludeZcOther = false;

      UTXOReservationManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager, const std::shared_ptr<ArmoryObject>& armory,
         const std::shared_ptr<spdlog::logger>& logger, QObject* parent = nullptr);
      ~UTXOReservationManager() override;

      UTXOReservationManager(const UTXOReservationManager &) = delete;
      UTXOReservationManager &operator=(const UTXOReservationManager &) = delete;

      UTXOReservationManager(UTXOReservationManager &&) = delete;
      UTXOReservationManager &operator=(UTXOReservationManager &&) = delete;

      // Shared code for both CC and Xbt reservation
      UtxoReservationToken makeNewReservation(const std::vector<UTXO> &utxos, const std::string &reserveId);
      UtxoReservationToken makeNewReservation(const std::vector<UTXO> &utxos);

      // Xbt specific implementation, each function defined two times
      // 1 - for hd wallet, and 2 - for hd leaf(wallet_id + purpose) which is needed for hw wallet
      void reserveBestXbtUtxoSet(const HDWalletId& walletId, BTCNumericTypes::satoshi_type quantity, bool partial,
         std::function<void(FixedXbtInputs&&)>&& cb, bool checkPbFeeFloor, CheckAmount checkAmount, bool includeZc);
      void reserveBestXbtUtxoSet(const HDWalletId& walletId, bs::hd::Purpose purpose,
         BTCNumericTypes::satoshi_type quantity, bool partial,
         std::function<void(FixedXbtInputs&&)>&& cb, bool checkPbFeeFloor, CheckAmount checkAmount);

      BTCNumericTypes::satoshi_type getAvailableXbtUtxoSum(const HDWalletId& walletId, bool includeZc) const;
      BTCNumericTypes::satoshi_type getAvailableXbtUtxoSum(const HDWalletId& walletId, bs::hd::Purpose purpose, bool includeZc) const;

      std::vector<UTXO> getAvailableXbtUTXOs(const HDWalletId& walletId, bool includeZc) const;
      std::vector<UTXO> getAvailableXbtUTXOs(const HDWalletId& walletId, bs::hd::Purpose purpose, bool includeZc) const;

      // If checkAmount is enabled but there are not enough UTXOs then callback will not be called
      void getBestXbtUtxoSet(const HDWalletId& walletId, BTCNumericTypes::satoshi_type quantity,
         std::function<void(std::vector<UTXO>&&)>&& cb, bool checkPbFeeFloor, CheckAmount checkAmount, bool includeZc);
      void getBestXbtUtxoSet(const HDWalletId& walletId, bs::hd::Purpose purpose, BTCNumericTypes::satoshi_type quantity,
         std::function<void(std::vector<UTXO>&&)>&& cb, bool checkPbFeeFloor, CheckAmount checkAmount);

      // CC specific implementation
      BTCNumericTypes::balance_type getAvailableCCUtxoSum(const CCProductName& CCProduct) const;
      std::vector<UTXO> getAvailableCCUTXOs(const CCWalletId& walletId) const;

      // Mutual functions
      FixedXbtInputs convertUtxoToFixedInput(const HDWalletId& walletId, const std::vector<UTXO>& utxos);
      FixedXbtInputs convertUtxoToPartialFixedInput(const HDWalletId& walletId, const std::vector<UTXO>& utxos);

      void setFeeRatePb(float feeRate);
      float feeRatePb() const;

   signals:
      void availableUtxoChanged(const std::string& walledId);

   private slots:
      void refreshAvailableUTXO();
      void onWalletsDeleted(const std::string& walledId);
      void onWalletsAdded(const std::string& walledId);
      void onWalletsBalanceChanged(const std::string& walledId);

   private:
      bool resetHdWallet(const std::string& hdWalledId);
      void resetSpendableXbt(const std::shared_ptr<bs::sync::hd::Wallet>& hdWallet);
      void resetSpendableCC(const std::shared_ptr<bs::sync::Wallet>& leaf);
      void resetAllSpendableCC(const std::shared_ptr<bs::sync::hd::Wallet>& hdWallet);
      void getBestXbtFromUtxos(const std::vector<UTXO> &selectedUtxo
         , BTCNumericTypes::satoshi_type quantity
         , std::function<void(std::vector<UTXO>&&)>&& cb, bool checkPbFeeFloor, CheckAmount checkAmount);

      std::function<void(std::vector<UTXO>&&)> getReservationCb(const HDWalletId& walletId, bool partial,
         std::function<void(FixedXbtInputs&&)>&& cb);

   private:
      struct XBTUtxoContainer {
         UtxoItemMap utxosLookup_;
      };

      std::unordered_map<HDWalletId, XBTUtxoContainer> availableXbtUTXOs_;
      std::unordered_map<CCWalletId, std::vector<UTXO>> availableCCUTXOs_;

      std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
      std::shared_ptr<ArmoryObject> armory_;
      std::shared_ptr<spdlog::logger> logger_;

      std::atomic<float> feeRatePb_{};
   };

}  // namespace bs

#endif // UTXO_RESERVATION_MANAGER_H
