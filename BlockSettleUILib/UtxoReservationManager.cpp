/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UtxoReservationManager.h"

#include <cassert>
#include <spdlog/spdlog.h>

#include "UtxoReservation.h"
#include "UtxoReservationToken.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "TradesUtils.h"

using namespace bs;

UTXOReservantionManager::UTXOReservantionManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager,
   const std::shared_ptr<spdlog::logger>& logger, QObject* parent /*= nullptr*/)
   : walletsManager_(walletsManager)
   , logger_(logger)
{
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized,
      this, &UTXOReservantionManager::refreshAvailableUTXO);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletAdded,
      this, &UTXOReservantionManager::refreshAvailableUTXO);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted,
      this, &UTXOReservantionManager::refreshAvailableUTXO);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceUpdated,
      this, &UTXOReservantionManager::refreshAvailableUTXO);
}

FixedXbtInputs UTXOReservantionManager::reserveBestUtxoSet(const std::string& walletId,
   const std::shared_ptr<bs::network::RFQ>& rfq, BTCNumericTypes::balance_type offer,
   const std::shared_ptr<UTXOReservantionManager>& reservationMgr)
{
   FixedXbtInputs fixedXbtInputs;

   if ((rfq->side == bs::network::Side::Sell && rfq->product != bs::network::XbtCurrency) ||
      (rfq->side == bs::network::Side::Buy && rfq->product == bs::network::XbtCurrency)) {
      return {}; // Nothing to reserve
   }

   auto &walletUtxos = availableUTXOs_[walletId];
   auto quantity = rfq->quantity;
   if (rfq->side == bs::network::Side::Buy) {
      if (rfq->assetType == bs::network::Asset::PrivateMarket) {
         quantity *= offer;
      }
      else if (rfq->assetType == bs::network::Asset::SpotXBT) {
         quantity /= offer;
      }
   }
   // #UTXOManager: for now we will get UTXO for selling in proportion 1.1 from sum we going to sell
   // this is temporary solution till the moment UTXOManager will be done
   quantity *= 1.1;

   std::vector<std::pair<BTCNumericTypes::balance_type, int>> utxoBalances;
   utxoBalances.reserve(availableUTXOs_.size());
   for (int i = 0; i < walletUtxos.size(); ++i) {
      utxoBalances.push_back({ bs::XBTAmount(walletUtxos[i].getValue()).GetValueBitcoin(), i});
   }

   std::sort(utxoBalances.begin(), utxoBalances.end(),
      [](auto const &left, auto const &right) -> bool {
      return left.first > right.first;
   });

   std::vector<int> bestSet;
   BTCNumericTypes::balance_type sum = utxoBalances[0].first;
   if (sum > quantity) {
      int i = 1;
      for (; i < utxoBalances.size(); ++i) {
         if (utxoBalances[i].first < quantity) {
            break;
         }
      }
      bestSet.push_back(utxoBalances[--i].second);
   }
   else {
      bestSet.push_back(utxoBalances[0].second);
      for (int i = 1; i < utxoBalances.size(); ++i) {
         if (sum + utxoBalances[i].first < quantity) {
            sum += utxoBalances[i].first;
            bestSet.push_back(utxoBalances[i].second);
            continue;
         }

         // Let's try to find even less suitable utxo quantity
         for (++i; i < utxoBalances.size(); ++i) {
            if (sum + utxoBalances[i].first < quantity) {
               break;
            }
         }

         bestSet.push_back(utxoBalances[--i].second);
         break;
      }
   }

   std::vector<UTXO> selectedUtxos;
   if (bestSet.size() == walletUtxos.size()) {
      selectedUtxos = std::move(walletUtxos);
      for (int i = 0; i < selectedUtxos.size(); ++i) {
         fixedXbtInputs.inputs.insert({ selectedUtxos[i], walletId });
      }
   }
   else {
      for (auto index : bestSet) {
         selectedUtxos.push_back(walletUtxos[index]);
         fixedXbtInputs.inputs.insert({ walletUtxos[index] , walletId});
      }
   }

   availableUTXOs_.clear();

   auto reserveId = fmt::format("rfq_reserve_{}", CryptoPRNG::generateRandom(8).toHexStr());
   fixedXbtInputs.utxoRes = bs::UtxoReservationToken::makeNewReservation(logger_, reservationMgr, selectedUtxos, reserveId);

   return fixedXbtInputs;
}

uint64_t bs::UTXOReservantionManager::getAvailableUtxoSum(const std::string& walletId) const
{
   uint64_t sum = 0;
   auto const& availableUtxos = availableUTXOs_.find(walletId);
   if (availableUtxos != availableUTXOs_.end()) {
      for (int i = 0; i < availableUtxos->second.size(); ++i) {
         sum += availableUtxos->second[i].getValue();
      }
   }
   return sum;
}

void bs::UTXOReservantionManager::refreshAvailableUTXO()
{
   if (!walletsManager_->isArmoryReady()) {
      return;
   }

   availableUTXOs_.clear();
   
   for (auto &wallet : walletsManager_->hdWallets()) {
      const auto &leaves = wallet->getGroup(wallet->getXBTGroupType())->getLeaves();
      std::vector<std::shared_ptr<bs::sync::Wallet>> wallets(leaves.begin(), leaves.end());

      bs::tradeutils::getSpendableTxOutList(wallets, [this, walletId = wallet->walletId()](const std::map<UTXO, std::string> &utxos) {
         std::map<std::string, std::vector<UTXO>> allUTXOs;
         for (const auto &utxo : utxos) {
            allUTXOs[walletId].push_back(utxo.first);
         }

         QMetaObject::invokeMethod(this, [this, allUTXOs = std::move(allUTXOs), walletId]{
            if (allUTXOs.empty()) {
               return;
            }

            availableUTXOs_.insert(std::make_move_iterator(begin(allUTXOs)),
               std::make_move_iterator(std::end(allUTXOs)));

            emit availableUtxoChanged(walletId);
         });
      });
   }
}
