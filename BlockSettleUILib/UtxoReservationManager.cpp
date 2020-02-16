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
#include "ArmoryObject.h"

using namespace bs;

UTXOReservationManager::UTXOReservationManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager,
   const std::shared_ptr<ArmoryObject>& armory, const std::shared_ptr<spdlog::logger>& logger, QObject* parent /*= nullptr*/)
   : walletsManager_(walletsManager)
   , armory_(armory)
   , logger_(logger)
{
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized,
      this, &UTXOReservationManager::refreshAvailableUTXO);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletAdded,
      this, &UTXOReservationManager::onWalletsAdded);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted,
      this, &UTXOReservationManager::onWalletsDeleted);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceUpdated,
      this, &UTXOReservationManager::onWalletsBalanceChanged);
}

void UTXOReservationManager::reserveBestUtxoSet(const std::string& walletId, BTCNumericTypes::satoshi_type quantity,
   std::function<void(FixedXbtInputs&&)>&& cb)
{
   auto bestUtxoSetCb = [mgr = QPointer<bs::UTXOReservationManager>(this), walletId, cbFixedXBT = std::move(cb)](std::vector<UTXO>&& utxos) {
      if (!mgr) {
         return;
      }

      FixedXbtInputs fixedXbtInputs = UTXOReservationManager::convertUtxoToFixedInput(walletId, utxos);
      fixedXbtInputs.utxoRes = mgr->makeNewReservation(utxos);

      cbFixedXBT(std::move(fixedXbtInputs));
   };

   getBestUtxoSet(walletId, quantity, bestUtxoSetCb);
}

UTXOReservationManager::~UTXOReservationManager() = default;

BTCNumericTypes::satoshi_type bs::UTXOReservationManager::getAvailableUtxoSum(const std::string& walletId) const
{
   BTCNumericTypes::satoshi_type sum = 0;
   auto const availableUtxos = getAvailableUTXOs(walletId);
   for (const auto &utxo : availableUtxos) {
      sum += utxo.getValue();
   }

   return sum;
}

std::vector<UTXO> bs::UTXOReservationManager::getAvailableUTXOs(const std::string& walletId) const
{
   std::vector<UTXO> utxos;
   auto const availableUtxos = availableUTXOs_.find(walletId);
   if (availableUtxos == availableUTXOs_.end()) {
      return {};
   }

   utxos = availableUtxos->second;
   UtxoReservation::instance()->filter(utxos);
   return utxos;
}

bs::UtxoReservationToken UTXOReservationManager::makeNewReservation(const std::vector<UTXO> &utxos, const std::string &reserveId)
{
   auto onReleaseCb = [mngr = QPointer<UTXOReservationManager>(this)]() {
      if (!mngr) {
         return;
      }
      mngr->availableUtxoChanged({});
   };

   auto reservation = bs::UtxoReservationToken::makeNewReservation(logger_, utxos, reserveId, onReleaseCb);
   // #ReservationMngr: could be optimized by updating only needed wallet
   availableUtxoChanged({});
   return reservation;
}

bs::UtxoReservationToken bs::UTXOReservationManager::makeNewReservation(const std::vector<UTXO> &utxos)
{
   auto reserveId = fmt::format("rfq_reserve_{}", CryptoPRNG::generateRandom(8).toHexStr());
   return makeNewReservation(utxos, reserveId);
}


void bs::UTXOReservationManager::refreshAvailableUTXO()
{
   availableUTXOs_.clear();
   for (auto &wallet : walletsManager_->hdWallets()) {
      onWalletsAdded(wallet->walletId());
   }
}

void bs::UTXOReservationManager::onWalletsDeleted(const std::string& walledId)
{
   availableUTXOs_.erase(walledId);
}

void bs::UTXOReservationManager::onWalletsAdded(const std::string& walledId)
{
   auto hdWallet = walletsManager_->getHDWalletById(walledId);
   bool isHdRoot = true;
   if (!hdWallet) {
      isHdRoot = false;
      hdWallet = walletsManager_->getHDRootForLeaf(walledId);

      if (!hdWallet) {
         return;
      }
   }

   const auto &leaves = hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves();
   std::vector<bs::sync::WalletsManager::WalletPtr> wallets(leaves.begin(), leaves.end());

   if (!isHdRoot) {
      auto it = std::find_if(wallets.cbegin(), wallets.cend(), [&walledId](const auto& wallet) -> bool {
         return wallet->walletId() == walledId;
      });

      if (it == wallets.end()) {
         return;
      }
   }

   bs::tradeutils::getSpendableTxOutList(wallets, [mgr = QPointer<bs::UTXOReservationManager>(this),
      walletId = hdWallet->walletId()](const std::map<UTXO, std::string> &utxos) {
      if (!mgr) {
         return; // manager thread die, nothing to do
      }

      std::vector<UTXO> walletUtxos;
      for (const auto &utxo : utxos) {
         walletUtxos.push_back(utxo.first);
      }

      QMetaObject::invokeMethod(mgr, [mgr, utxos = std::move(walletUtxos), id = walletId]{
         mgr->availableUTXOs_[id] = std::move(utxos);

         emit mgr->availableUtxoChanged(id);
      });
   }, false);
}

void bs::UTXOReservationManager::onWalletsBalanceChanged(const std::string& walledId)
{
   onWalletsDeleted(walledId);
   onWalletsAdded(walledId);
}

void bs::UTXOReservationManager::getBestUtxoSet(const std::string& walletId,
   BTCNumericTypes::satoshi_type quantity, std::function<void(std::vector<UTXO>&&)>&& cb)
{
   auto &walletUtxos = availableUTXOs_[walletId];

   std::vector<std::pair<BTCNumericTypes::satoshi_type, int>> utxoBalances;
   utxoBalances.reserve(walletUtxos.size());
   for (int i = 0; i < walletUtxos.size(); ++i) {
      utxoBalances.push_back({ walletUtxos[i].getValue(), i });
   }

   std::sort(utxoBalances.begin(), utxoBalances.end(),
      [](auto const &left, auto const &right) -> bool {
      return left.first > right.first;
   });

   std::vector<int> bestSet;
   BTCNumericTypes::satoshi_type sum = utxoBalances[0].first;
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
      cb(std::move(selectedUtxos)); // No need to calculate fee - we are going to spend all utxo from wallet now 
      return;
   }
   else {
      for (auto index : bestSet) {
         selectedUtxos.push_back(walletUtxos[index]);
      }
   }

   // Here we calculating fee based on chosen utxos, if total price with fee will cover by all utxo sum - then we good and could continue
   // otherwise let's try to find better set of utxo again till the moment we will cover the difference or use all available utxos from wallet
   auto feeCb = [mgr = QPointer<bs::UTXOReservationManager>(this), quantity, walletId, utxos = std::move(selectedUtxos), cbCopy = std::move(cb)](float fee) mutable {
      if (!mgr) {
         return; // main thread die, nothing to do
      }

      QMetaObject::invokeMethod(mgr, [mgr, quantity, walletId, fee, utxos = std::move(utxos), cb = std::move(cbCopy)] () mutable {
         float feePerByte = ArmoryConnection::toFeePerByte(fee);
         BTCNumericTypes::satoshi_type total = 0;
         for (const auto &utxo : utxos) {
            total += utxo.getValue();
         }
         const auto fee = bs::tradeutils::estimatePayinFeeWithoutChange(utxos, feePerByte);

         const BTCNumericTypes::satoshi_type spendableQuantity = quantity + fee;
         if (spendableQuantity > total) {
            mgr->getBestUtxoSet(walletId, spendableQuantity, std::move(cb));
         }
         else {
            cb(std::move(utxos));
         }
      });
   };
   armory_->estimateFee(bs::tradeutils::feeTargetBlockCount(), feeCb);
}

bs::FixedXbtInputs UTXOReservationManager::convertUtxoToFixedInput(const std::string& walletId, const std::vector<UTXO>& utxos)
{
   FixedXbtInputs fixedXbtInputs;
   for (auto utxo : utxos) {
      fixedXbtInputs.inputs.insert({ utxo, walletId });
   }
   return fixedXbtInputs;
}
