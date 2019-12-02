/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SyncWalletsManager.h"

#include "ApplicationSettings.h"
#include "CheckRecipSigner.h"
#include "CoinSelection.h"
#include "ColoredCoinLogic.h"
#include "FastLock.h"
#include "PublicResolver.h"
#include "SyncHDWallet.h"

#include <QCoreApplication>
#include <QDir>
#include <QMutexLocker>

#include <spdlog/spdlog.h>

using namespace bs::sync;
using namespace bs::signer;

bool isCCNameCorrect(const std::string& ccName)
{
   if ((ccName.length() == 1) && (ccName[0] >= '0') && (ccName[0] <= '9')) {
      return false;
   }

   return true;
}

WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<ArmoryConnection> &armory)
   : QObject(nullptr)
   , logger_(logger)
   , appSettings_(appSettings)
   , armoryPtr_(armory)
{
   init(armory.get());

   ccResolver_ = std::make_shared<CCResolver>();
   maintThreadRunning_ = true;
   maintThread_ = std::thread(&WalletsManager::maintenanceThreadFunc, this);
}

WalletsManager::~WalletsManager() noexcept
{
   validityFlag_.reset();

   for (const auto &hdWallet : hdWallets_) {
      hdWallet->setWCT(nullptr);
   }
   {
      std::unique_lock<std::mutex> lock(maintMutex_);
      maintThreadRunning_ = false;
      maintCV_.notify_one();
   }
   if (maintThread_.joinable()) {
      maintThread_.join();
   }

   cleanup();
}

void WalletsManager::setSignContainer(const std::shared_ptr<WalletSignerContainer> &container)
{
   signContainer_ = container;

   connect(signContainer_.get(), &WalletSignerContainer::AuthLeafAdded, this, &WalletsManager::onAuthLeafAdded);
   connect(signContainer_.get(), &WalletSignerContainer::walletsListUpdated, this, &WalletsManager::onWalletsListUpdated);
}

void WalletsManager::reset()
{
   wallets_.clear();
   hdWallets_.clear();
//   hdDummyWallet_.reset();
   walletNames_.clear();
   readyWallets_.clear();
   isReady_ = false;
   authAddressWallet_.reset();

   emit walletChanged("");
}

void WalletsManager::syncWallet(const bs::sync::WalletInfo &info, const std::function<void()> &cbDone)
{
   logger_->debug("[WalletsManager::syncWallets] syncing wallet {} ({} {})"
      , info.id, info.name, (int)info.format);

   switch (info.format) {
   case bs::sync::WalletFormat::HD:
   {
      try {
         const auto hdWallet = std::make_shared<hd::Wallet>(info, signContainer_.get(), logger_);
         hdWallet->setWCT(this);

         if (hdWallet) {
            const auto &cbHDWalletDone = [this, hdWallet, cbDone] {
               logger_->debug("[WalletsManager::syncWallets] synced HD wallet {}"
                  , hdWallet->walletId());
               saveWallet(hdWallet);
               cbDone();
            };
            hdWallet->synchronize(cbHDWalletDone);
         }
      } catch (const std::exception &e) {
         logger_->error("[WalletsManager::syncWallets] failed to create HD wallet "
            "{}: {}", info.id, e.what());
         cbDone();
      }
      break;
   }

   case bs::sync::WalletFormat::Settlement:
      throw std::runtime_error("not implemented");
      break;

   default:
      cbDone();
      logger_->info("[WalletsManager::syncWallets] wallet format {} is not "
         "supported yet", (int)info.format);
      break;
   }
}

void WalletsManager::syncWallets(const CbProgress &cb)
{
   const auto &cbWalletInfo = [this, cb](const std::vector<bs::sync::WalletInfo> &wi) {
      auto walletIds = std::make_shared<std::unordered_set<std::string>>();
      for (const auto &info : wi)
         walletIds->insert(info.id);

      for (const auto &info : wi) {
         const auto &cbDone = [this, walletIds, id = info.id, total = wi.size(), cb]
         {
            walletIds->erase(id);
            if (cb)
               cb(total - walletIds->size(), total);

            if (walletIds->empty()) {
               logger_->debug("[WalletsManager::syncWallets] all wallets synchronized");
               emit walletsSynchronized();
               emit walletChanged("");
               synchronized_ = true;
            }
         };

         syncWallet(info, cbDone);
      }

      logger_->debug("[WalletsManager::syncWallets] initial wallets synchronized");
      if (wi.empty()) {
         emit walletDeleted("");
      }

      if (wi.empty()) {
         synchronized_ = true;
         emit walletsSynchronized();
      }
   };

   synchronized_ = false;
   if (!signContainer_) {
      logger_->error("[WalletsManager::{}] signer is not set - aborting"
         , __func__);
      return;
   }
   signContainer_->syncWalletInfo(cbWalletInfo);
}

bool WalletsManager::isWalletsReady() const
{
   if (synchronized_ && hdWallets_.empty()) {
      return true;
   }
   return isReady_;
}

bool WalletsManager::isReadyForTrading() const
{
   return hasPrimaryWallet();
}

void WalletsManager::saveWallet(const WalletPtr &newWallet)
{
/*   if (hdDummyWallet_ == nullptr) {
      hdDummyWallet_ = std::make_shared<hd::DummyWallet>(logger_);
      hdWalletsId_.insert(hdDummyWallet_->walletId());
      hdWallets_[hdDummyWallet_->walletId()] = hdDummyWallet_;
   }*/
   addWallet(newWallet);
}

// XXX should it be register in armory ?
// XXX should it start rescan ?
void WalletsManager::addWallet(const WalletPtr &wallet, bool isHDLeaf)
{
/*   if (!isHDLeaf && hdDummyWallet_)
      hdDummyWallet_->add(wallet);
*/
   auto ccLeaf = std::dynamic_pointer_cast<bs::sync::hd::CCLeaf>(wallet);
   if (ccLeaf) {
      ccLeaf->setCCDataResolver(ccResolver_);
      updateTracker(ccLeaf);
   }
   wallet->setUserId(userId_);

   {
      QMutexLocker lock(&mtxWallets_);
      wallets_[wallet->walletId()] = wallet;
   }

   if (isHDLeaf && (wallet->type() == bs::core::wallet::Type::Authentication)) {
      authAddressWallet_ = wallet;
      logger_->debug("[WalletsManager] auth leaf changed/created");
      emit AuthLeafCreated();
      emit authWalletChanged();
   }
}

void WalletsManager::balanceUpdated(const std::string &walletId)
{
   addToMaintQueue([this, walletId] {
      QMetaObject::invokeMethod(this, [this, walletId] { emit walletBalanceUpdated(walletId); });
   });
}

void WalletsManager::addressAdded(const std::string &walletId)
{
   addToMaintQueue([this, walletId] {
      QMetaObject::invokeMethod(this, [this, walletId] { emit walletChanged(walletId); });
   });
}

void WalletsManager::metadataChanged(const std::string &walletId)
{
   addToMaintQueue([this, walletId] {
      QMetaObject::invokeMethod(this, [this, walletId] { emit walletMetaChanged(walletId); });
   });
}

void WalletsManager::walletReset(const std::string &walletId)
{
   addToMaintQueue([this, walletId] {
      QMetaObject::invokeMethod(this, [this, walletId] { emit walletChanged(walletId); });
   });
}

void WalletsManager::saveWallet(const HDWalletPtr &wallet)
{
   if (!userId_.isNull()) {
      wallet->setUserId(userId_);
   }
   const auto existingHdWallet = getHDWalletById(wallet->walletId());

   if (existingHdWallet) {    // merge if HD wallet already exists
      existingHdWallet->merge(*wallet);
   }
   else {
      hdWallets_.push_back(wallet);
   }

   for (const auto &leaf : wallet->getLeaves()) {
      addWallet(leaf, true);
   }
}

void WalletsManager::walletCreated(const std::string &walletId)
{
   logger_->debug("[{}] walletId={}", __func__, walletId);
   return;
   const auto &lbdMaint = [this, walletId] {
      for (const auto &hdWallet : hdWallets_) {
         const auto leaf = hdWallet->getLeaf(walletId);
         if (leaf == nullptr) {
            continue;
         }
         logger_->debug("[WalletsManager::walletCreated] HD leaf {} ({}) added"
            , walletId, leaf->name());

         addWallet(leaf);

         QMetaObject::invokeMethod(this, [this, walletId] { emit walletChanged(walletId); });
         break;
      }
   };
   addToMaintQueue(lbdMaint);
}

void WalletsManager::walletDestroyed(const std::string &walletId)
{
   addToMaintQueue([this, walletId] {
      const auto &wallet = getWalletById(walletId);
      eraseWallet(wallet);
      QMetaObject::invokeMethod(this, [this, walletId] { emit walletChanged(walletId); });
   });
}

WalletsManager::HDWalletPtr WalletsManager::getPrimaryWallet() const
{
   for (const auto &wallet : hdWallets_) {
      if (wallet->isPrimary()) {
         return wallet;
      }
   }
   return nullptr;
}

bool WalletsManager::hasPrimaryWallet() const
{
   return (getPrimaryWallet() != nullptr);
}

WalletsManager::WalletPtr WalletsManager::getDefaultWallet() const
{
   WalletPtr result;
   const auto &priWallet = getPrimaryWallet();
   if (priWallet) {
      const auto &group = priWallet->getGroup(priWallet->getXBTGroupType());

      //all leaf paths are always hardened
      const bs::hd::Path leafPath({ bs::hd::Purpose::Native, priWallet->getXBTGroupType(), 0});
      result = group ? group->getLeaf(leafPath) : nullptr;
   }
   return result;
}

WalletsManager::WalletPtr WalletsManager::getCCWallet(const std::string &cc)
{
   if (cc.empty() || !hasPrimaryWallet()) {
      return nullptr;
   }

   if (!isCCNameCorrect(cc)) {
      logger_->error("[WalletsManager::getCCWallet] invalid cc name passed: {}"
                     , cc);
      return nullptr;
   }

   const auto &priWallet = getPrimaryWallet();
   auto ccGroup = priWallet->getGroup(bs::hd::CoinType::BlockSettle_CC);
   if (ccGroup == nullptr) {
      //cc wallet is always ext only
      ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC, true);
   }
   const bs::hd::Path ccLeafPath({ bs::hd::Purpose::Native, bs::hd::CoinType::BlockSettle_CC
      , bs::hd::Path::keyToElem(cc) });
   return ccGroup->getLeaf(ccLeafPath);
}

bool WalletsManager::isValidCCOutpoint(const std::string &cc, const BinaryData &txHash
   , uint32_t, uint64_t value) const
{  // not using txOutIndex is intended now, as it won't return correct results
   const auto &itTracker = trackers_.find(cc);
   if (itTracker == trackers_.end()) {
      return false;
   }
/*   const bool result = itTracker->second->isTxHashValid(txHash);
   if (!result) {          // FIXME: disabled temporarily until CC tracker
      return false;        // will allow to validate any spent UTXO
   }*/
   return ((value % ccResolver_->lotSizeFor(cc)) == 0);
}

void WalletsManager::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   for (const auto &hdWallet : hdWallets_) {
      hdWallet->setUserId(userId);
   }
   auto primaryWallet = getPrimaryWallet();
   if (signContainer_) {
      signContainer_->setUserId(userId, primaryWallet ? primaryWallet->walletId() : "");
   }
}

const WalletsManager::HDWalletPtr WalletsManager::getHDWalletById(const std::string& walletId) const
{
   auto it = std::find_if(hdWallets_.cbegin(), hdWallets_.cend(), [walletId](const HDWalletPtr &hdWallet) {
      return (hdWallet->walletId() == walletId);
   });
   if (it != hdWallets_.end()) {
      return *it;
   }
   return nullptr;
}

const WalletsManager::HDWalletPtr WalletsManager::getHDRootForLeaf(const std::string& walletId) const
{
   for (const auto &hdWallet : hdWallets_) {
      for (const auto &leaf : hdWallet->getLeaves()) {
         if (leaf->hasId(walletId)) {
            return hdWallet;
         }
      }
   }
   return nullptr;
}

std::vector<WalletsManager::WalletPtr> WalletsManager::getAllWallets() const
{
   std::vector<WalletPtr> result;
   for (const auto &wallet : wallets_) {
      result.push_back(wallet.second);
   }
   return result;
}

WalletsManager::WalletPtr WalletsManager::getWalletById(const std::string& walletId) const
{
   for (const auto &wallet : wallets_) {
      if (wallet.second->hasId(walletId)) {
         return wallet.second;
      }
   }
   return nullptr;
}

WalletsManager::WalletPtr WalletsManager::getWalletByAddress(const bs::Address &address) const
{
   for (const auto &wallet : wallets_) {
      if (wallet.second && (wallet.second->containsAddress(address)
         || wallet.second->containsHiddenAddress(address))) {
         return wallet.second;
      }
   }
   return nullptr;
}

WalletsManager::GroupPtr WalletsManager::getGroupByWalletId(const std::string& walletId) const
{
   const auto itGroup = groupsByWalletId_.find(walletId);
   if (itGroup == groupsByWalletId_.end()) {
      const auto hdWallet = getHDRootForLeaf(walletId);
      if (hdWallet) {
         for (const auto &group : hdWallet->getGroups()) {
            for (const auto &leaf : group->getLeaves()) {
               if (leaf->hasId(walletId)) {
                  groupsByWalletId_[walletId] = group;
                  return group;
               }
            }
         }
      }
      groupsByWalletId_[walletId] = nullptr;
      return nullptr;
   }
   return itGroup->second;
}

bool WalletsManager::walletNameExists(const std::string &walletName) const
{
   const auto &it = walletNames_.find(walletName);
   return (it != walletNames_.end());
}

BTCNumericTypes::balance_type WalletsManager::getSpendableBalance() const
{
   if (!isArmoryReady()) {
      return std::numeric_limits<double>::infinity();
   }
   // TODO: make it lazy init
   BTCNumericTypes::balance_type totalSpendable = 0;

   for (const auto& it : wallets_) {
      if (it.second->type() != bs::core::wallet::Type::Bitcoin) {
         continue;
      }
      const auto walletSpendable = it.second->getSpendableBalance();
      if (walletSpendable > 0) {
         totalSpendable += walletSpendable;
      }
   }
   return totalSpendable;
}

BTCNumericTypes::balance_type WalletsManager::getUnconfirmedBalance() const
{
   return getBalanceSum([](const WalletPtr &wallet) {
      return wallet->type() == core::wallet::Type::Bitcoin ? wallet->getUnconfirmedBalance() : 0;
   });
}

BTCNumericTypes::balance_type WalletsManager::getTotalBalance() const
{
   return getBalanceSum([](const WalletPtr &wallet) {
      return wallet->type() == core::wallet::Type::Bitcoin ? wallet->getTotalBalance() : 0;
   });
}

BTCNumericTypes::balance_type WalletsManager::getBalanceSum(
   const std::function<BTCNumericTypes::balance_type(const WalletPtr &)> &cb) const
{
   if (!isArmoryReady()) {
      return 0;
   }
   BTCNumericTypes::balance_type balance = 0;

   for (const auto& it : wallets_) {
      balance += cb(it.second);
   }
   return balance;
}

void WalletsManager::onNewBlock(unsigned int, unsigned int)
{
   QMetaObject::invokeMethod(this, [this] {emit blockchainEvent(); });
}

void WalletsManager::onStateChanged(ArmoryState state)
{
   if (state == ArmoryState::Ready) {
      logger_->debug("[{}] DB ready", __func__);
   }
   else {
      logger_->debug("[WalletsManager::{}] -  Armory state changed: {}"
         , __func__, (int)state);
   }
}

void WalletsManager::walletReady(const std::string &walletId)
{
   QMetaObject::invokeMethod(this, [this, walletId] { emit walletIsReady(walletId); });
   const auto rootWallet = getHDRootForLeaf(walletId);
   if (rootWallet) {
      const auto &itWallet = newWallets_.find(rootWallet->walletId());
      if (itWallet != newWallets_.end()) {
         logger_->debug("[{}] found new wallet {} - starting rescan", __func__, rootWallet->walletId());
         rootWallet->startRescan();
         newWallets_.erase(itWallet);
         rootWallet->synchronize([this, rootWallet] {
            QMetaObject::invokeMethod(this, [this, rootWallet] {
               for (const auto &leaf : rootWallet->getLeaves()) {
                  addWallet(leaf, true);
               }
               emit walletAdded(rootWallet->walletId());
               emit walletsReady();
               logger_->debug("[WalletsManager] wallets are ready after rescan");
            });
         });
      }
      else {
         logger_->debug("[{}] wallet {} completed registration", __func__, walletId);
         emit walletBalanceUpdated(walletId);
      }
   }

   readyWallets_.insert(walletId);
   auto nbWallets = wallets_.size();
   if (readyWallets_.size() >= nbWallets) {
      isReady_ = true;
      logger_->debug("[WalletsManager::{}] All wallets are ready", __func__);
      emit walletsReady();
      readyWallets_.clear();
   }
}

void WalletsManager::scanComplete(const std::string &walletId)
{
   logger_->debug("[{}] - HD wallet {} imported", __func__, walletId);
   const auto hdWallet = getHDWalletById(walletId);
   if (hdWallet) {
      hdWallet->registerWallet(armoryPtr_);
   }
   QMetaObject::invokeMethod(this, [this, walletId] {
      emit walletChanged(walletId);
      emit walletImportFinished(walletId);
   });
}

bool WalletsManager::isArmoryReady() const
{
   return (armory_ && (armory_->state() == ArmoryState::Ready));
}

void WalletsManager::eraseWallet(const WalletPtr &wallet)
{
   if (!wallet) {
      return;
   }
   QMutexLocker lock(&mtxWallets_);
   wallets_.erase(wallet->walletId());
}

bool WalletsManager::deleteWallet(WalletPtr wallet)
{
   bool isHDLeaf = false;
   logger_->info("[WalletsManager::{}] - Removing wallet {} ({})...", __func__
      , wallet->name(), wallet->walletId());
   for (auto hdWallet : hdWallets_) {
      const auto leaves = hdWallet->getLeaves();
      if (std::find(leaves.begin(), leaves.end(), wallet) != leaves.end()) {
         for (auto group : hdWallet->getGroups()) {
            if (group->deleteLeaf(wallet)) {
               isHDLeaf = true;
               signContainer_->DeleteHDLeaf(wallet->walletId());
               eraseWallet(wallet);
               break;
            }
         }
      }
      if (isHDLeaf) {
         break;
      }
   }

   wallet->unregisterWallet();
   if (!isHDLeaf) {
      eraseWallet(wallet);
   }

   if (authAddressWallet_ == wallet) {
      authAddressWallet_ = nullptr;
      emit authWalletChanged();
   }
   emit walletDeleted(wallet->walletId());
   emit walletBalanceUpdated(wallet->walletId());
   return true;
}

bool WalletsManager::deleteWallet(HDWalletPtr wallet)
{
   const auto itHdWallet = std::find(hdWallets_.cbegin(), hdWallets_.cend(), wallet);
   if (itHdWallet == hdWallets_.end()) {
      logger_->warn("[WalletsManager::{}] - Unknown HD wallet {} ({})", __func__
         , wallet->name(), wallet->walletId());
      return false;
   }

   const auto &leaves = wallet->getLeaves();
   const bool prevState = blockSignals(true);
   for (const auto &leaf : leaves) {
      leaf->unregisterWallet();
   }
   for (const auto &leaf : leaves) {
      eraseWallet(leaf);
   }
   blockSignals(prevState);

   hdWallets_.erase(itHdWallet);
   walletNames_.erase(wallet->name());

   const bool result = wallet->deleteRemotely();
   logger_->info("[WalletsManager::{}] - Wallet {} ({}) removed: {}", __func__
      , wallet->name(), wallet->walletId(), result);

   if (!getPrimaryWallet()) {
      authAddressWallet_.reset();
      emit authWalletChanged();
   }
   emit walletDeleted(wallet->walletId());
   emit walletBalanceUpdated(wallet->walletId());
   return result;
}

std::vector<std::string> WalletsManager::registerWallets()
{
   std::vector<std::string> result;
   if (!armory_) {
      logger_->warn("[WalletsManager::{}] armory is not set", __func__);
      return result;
   }
   if (empty()) {
      logger_->debug("[WalletsManager::{}] no wallets to register", __func__);
      return result;
   }
   for (auto &it : wallets_) {
      const auto &ids = it.second->registerWallet(armoryPtr_);
      result.insert(result.end(), ids.begin(), ids.end());
      if (ids.empty() && it.second->type() != bs::core::wallet::Type::Settlement) {
         logger_->error("[{}] failed to register wallet {}", __func__, it.second->walletId());
      }
   }

   return result;
}

void WalletsManager::unregisterWallets()
{
   for (auto &it : wallets_) {
      it.second->unregisterWallet();
   }
}

bool WalletsManager::getTransactionDirection(Tx tx, const std::string &walletId
   , const std::function<void(Transaction::Direction, std::vector<bs::Address>)> &cb)
{
   if (!tx.isInitialized()) {
      logger_->error("[WalletsManager::{}] TX not initialized", __func__);
      return false;
   }

   if (!armory_) {
      logger_->error("[WalletsManager::{}] armory not set", __func__);
      return false;
   }

   const auto wallet = getWalletById(walletId);
   if (!wallet) {
      logger_->error("[WalletsManager::{}] failed to get wallet for id {}"
         , __func__, walletId);
      return false;
   }

   if (wallet->type() == bs::core::wallet::Type::Authentication) {
      cb(Transaction::Auth, {});
      return true;
   }
   else if (wallet->type() == bs::core::wallet::Type::ColorCoin) {
      cb(Transaction::Delivery, {});
      return true;
   }

   const auto group = getGroupByWalletId(walletId);
   if (!group) {
      logger_->warn("[{}] group for {} not found", __func__, walletId);
   }

   const std::string txKey = tx.getThisHash().toBinStr() + walletId;
   auto dir = Transaction::Direction::Unknown;
   std::vector<bs::Address> inAddrs;
   {
      FastLock lock(txDirLock_);
      const auto &itDirCache = txDirections_.find(txKey);
      if (itDirCache != txDirections_.end()) {
         dir = itDirCache->second.first;
         inAddrs = itDirCache->second.second;
      }
   }
   if (dir != Transaction::Direction::Unknown) {
      cb(dir, inAddrs);
      return true;
   }

   std::set<BinaryData> opTxHashes;
   std::map<BinaryData, std::vector<uint32_t>> txOutIndices;

   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy((int)i);
      OutPoint op = in.getOutPoint();

      opTxHashes.insert(op.getTxHash());
      txOutIndices[op.getTxHash()].push_back(op.getTxOutIndex());
   }

   const auto &cbProcess = [this, wallet, group, tx, txKey, txOutIndices, cb]
      (const std::vector<Tx> &txs, std::exception_ptr)
   {
      bool ourOuts = false;
      bool otherOuts = false;
      bool ourIns = false;
      bool otherIns = false;
      bool ccTx = false;

      std::vector<TxOut> txOuts;
      std::vector<bs::Address> inAddrs;
      txOuts.reserve(tx.getNumTxIn());
      inAddrs.reserve(tx.getNumTxIn());

      for (const auto &prevTx : txs) {
         const auto &itIdx = txOutIndices.find(prevTx.getThisHash());
         if (itIdx == txOutIndices.end()) {
            continue;
         }
         for (const auto idx : itIdx->second) {
            TxOut prevOut = prevTx.getTxOutCopy((int)idx);
            const auto addr = bs::Address::fromTxOut(prevOut);
            const auto addrWallet = getWalletByAddress(addr);
            const auto addrGroup = addrWallet ? getGroupByWalletId(addrWallet->walletId()) : nullptr;
            (((addrWallet == wallet) || (group && (group == addrGroup))) ? ourIns : otherIns) = true;
            if (addrWallet && (addrWallet->type() == bs::core::wallet::Type::ColorCoin)) {
               ccTx = true;
            }
            txOuts.emplace_back(prevOut);
            inAddrs.emplace_back(std::move(addr));
         }
      }

      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy((int)i);
         const auto addrObj = bs::Address::fromHash(out.getScrAddressStr());
         const auto addrWallet = getWalletByAddress(addrObj);
         const auto addrGroup = addrWallet ? getGroupByWalletId(addrWallet->walletId()) : nullptr;
         (((addrWallet == wallet) || (group && (group == addrGroup))) ? ourOuts : otherOuts) = true;
         if (addrWallet && (addrWallet->type() == bs::core::wallet::Type::ColorCoin)) {
            ccTx = true;
            break;
         }
         else if (!ourOuts) {
            if ((group && addrGroup) && (group == addrGroup)) {
               ourOuts = true;
               otherOuts = false;
            }
         }
      }

      if (wallet->type() == bs::core::wallet::Type::Settlement) {
         if (ourOuts) {
            updateTxDirCache(txKey, Transaction::PayIn, inAddrs, cb);
            return;
         }
         if (txOuts.size() == 1) {
#if 0    //TODO: decide later how to handle settlement addresses
            const bs::Address addr = txOuts[0].getScrAddressStr();
            const auto settlAE = getSettlementWallet()->getAddressEntryForAddr(addr);
            if (settlAE) {
               const auto &cbPayout = [this, cb, txKey, inAddrs](bs::PayoutSigner::Type poType) {
                  if (poType == bs::PayoutSigner::SignedBySeller) {
                     updateTxDirCache(txKey, Transaction::Revoke, inAddrs, cb);
                  }
                  else {
                     updateTxDirCache(txKey, Transaction::PayOut, inAddrs, cb);
                  }
               };
               bs::PayoutSigner::WhichSignature(tx, 0, settlAE, logger_, armoryPtr_, cbPayout);
               return;
            }
            logger_->warn("[WalletsManager::{}] - failed to get settlement AE"
               , __func__);
#endif   //0
         }
         else {
            logger_->warn("[WalletsManager::{}] - more than one settlement "
               "output", __func__);
         }
         updateTxDirCache(txKey, Transaction::PayOut, inAddrs, cb);
         return;
      }

      if (ccTx) {
         updateTxDirCache(txKey, Transaction::Payment, inAddrs, cb);
         return;
      }
      if (ourOuts && ourIns && !otherOuts && !otherIns) {
         updateTxDirCache(txKey, Transaction::Internal, inAddrs, cb);
         return;
      }
      if (!ourIns) {
         updateTxDirCache(txKey, Transaction::Received, inAddrs, cb);
         return;
      }
      if (otherOuts) {
         updateTxDirCache(txKey, Transaction::Sent, inAddrs, cb);
         return;
      }
      updateTxDirCache(txKey, Transaction::Unknown, inAddrs, cb);
   };
   if (opTxHashes.empty()) {
      logger_->error("[WalletsManager::{}] - empty TX hashes", __func__);
      return false;
   }
   else {
      armory_->getTXsByHash(opTxHashes, cbProcess);
   }
   return true;
}

bool WalletsManager::getTransactionMainAddress(const Tx &tx, const std::string &walletId
   , bool isReceiving, const std::function<void(QString, int)> &cb)
{
   if (!tx.isInitialized() || !armory_) {
      return false;
   }
   const auto wallet = getWalletById(walletId);
   if (!wallet) {
      return false;
   }

   const std::string txKey = tx.getThisHash().toBinStr() + walletId;
   const auto &itDesc = txDesc_.find(txKey);
   if (itDesc != txDesc_.end()) {
      cb(itDesc->second.first, itDesc->second.second);
      return true;
   }

   const bool isSettlement = (wallet->type() == bs::core::wallet::Type::Settlement);
   std::set<bs::Address> ownAddresses, foreignAddresses;
   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy((int)i);
      try {
         const auto addr = bs::Address::fromTxOut(out);
         const auto addrWallet = getWalletByAddress(addr);
         if (addrWallet == wallet) {
            ownAddresses.insert(addr);
         } else {
            foreignAddresses.insert(addr);
         }
      }
      catch (const std::exception &) {
         // address conversion failure - likely OP_RETURN - do nothing
      }
   }

   if (!isReceiving && (ownAddresses.size() == 1) && !foreignAddresses.empty()) {
      if (!wallet->isExternalAddress(*ownAddresses.begin())) {
         ownAddresses.clear();   // treat the only own internal address as change and throw away
      }
   }

   const auto &lbdProcessAddresses = [this, txKey, cb](const std::set<bs::Address> &addresses) {
      switch (addresses.size()) {
      case 0:
         updateTxDescCache(txKey, tr("no address"), (int)addresses.size(), cb);
         break;

      case 1:
         updateTxDescCache(txKey, QString::fromStdString((*addresses.begin()).display())
            , (int)addresses.size(), cb);
         break;

      default:
         updateTxDescCache(txKey, tr("%1 output addresses").arg(addresses.size()), (int)addresses.size(), cb);
         break;
      }
   };

   if (!ownAddresses.empty()) {
      lbdProcessAddresses(ownAddresses);
   }
   else {
      lbdProcessAddresses(foreignAddresses);
   }
   return true;
}

void WalletsManager::updateTxDirCache(const std::string &txKey, Transaction::Direction dir
   , const std::vector<bs::Address> &inAddrs
   , std::function<void(Transaction::Direction, std::vector<bs::Address>)> cb)
{
   {
      FastLock lock(txDirLock_);
      txDirections_[txKey] = { dir, inAddrs };
   }
   cb(dir, inAddrs);
}

void WalletsManager::updateTxDescCache(const std::string &txKey, const QString &desc, int addrCount, std::function<void(QString, int)> cb)
{
   {
      FastLock lock(txDescLock_);
      txDesc_[txKey] = { desc, addrCount };
   }
   cb(desc, addrCount);
}

void WalletsManager::createSettlementLeaf(const bs::Address &authAddr
   , const std::function<void(const SecureBinaryData &)> &cb)
{
   if (!signContainer_) {
      logger_->error("[{}] signer is not set - aborting", __func__);
      if (cb) {
         cb({});
      }
      return;
   }
   const auto cbWrap = [this, cb](const SecureBinaryData &pubKey) {
      const auto priWallet = getPrimaryWallet();
      if (!priWallet) {
         logger_->error("[WalletsManager::createSettlementLeaf] no primary wallet");
         return;
      }
      priWallet->synchronize([this, priWallet] {
         const auto group = priWallet->getGroup(bs::hd::BlockSettle_Settlement);
         if (!group) {
            logger_->error("[WalletsManager::createSettlementLeaf] no settlement group");
            return;
         }
         for (const auto &settlLeaf : group->getLeaves()) {
            if (getWalletById(settlLeaf->walletId()) != nullptr) {
               logger_->warn("[WalletsManager::createSettlementLeaf] leaf {} already exists", settlLeaf->walletId());
               continue;
            }
            addWallet(settlLeaf, true);
            emit walletAdded(settlLeaf->walletId());
         }
      });
      if (cb) {
         cb(pubKey);
      }
   };
   signContainer_->createSettlementWallet(authAddr, cbWrap);
}

void WalletsManager::onHDWalletCreated(unsigned int id, std::shared_ptr<bs::sync::hd::Wallet> newWallet)
{
   if (id != createHdReqId_) {
      return;
   }
   createHdReqId_ = 0;
   newWallet->synchronize([] {});
   adoptNewWallet(newWallet);
   emit walletAdded(newWallet->walletId());
}

void WalletsManager::startWalletRescan(const HDWalletPtr &hdWallet)
{
   if (armory_->state() == ArmoryState::Ready) {
      hdWallet->startRescan();
   }
   else {
      logger_->error("[{}] invalid Armory state {}", __func__, (int)armory_->state());
   }
}

void WalletsManager::onWalletsListUpdated()
{
   const auto &cbSyncWallets = [this](const std::vector<bs::sync::WalletInfo> &wi) {
      std::map<std::string, bs::sync::WalletInfo> hdWallets;
      for (const auto &info : wi) {
         hdWallets[info.id] = info;
      }
      for (const auto &hdWallet : hdWallets) {
         const auto &itHdWallet = std::find_if(hdWallets_.cbegin(), hdWallets_.cend()
            , [walletId=hdWallet.first](const HDWalletPtr &wallet) {
               return (wallet->walletId() == walletId);
         });
         if (itHdWallet == hdWallets_.end()) {
            syncWallet(hdWallet.second, [this, hdWalletId=hdWallet.first]
            {
               const auto hdWallet = getHDWalletById(hdWalletId);
               if (hdWallet) {
                  hdWallet->registerWallet(armoryPtr_);
               }
            });
            newWallets_.insert(hdWallet.first);
         }
         else {
            const auto wallet = *itHdWallet;
            const auto &cbSyncHD = [this, wallet](bs::sync::HDWalletData hdData) {
               bool walletUpdated = false;
               if (hdData.groups.size() != wallet->getGroups().size()) {
                  walletUpdated = true;
               }
               else {
                  for (const auto &group : hdData.groups) {
                     const auto hdGroup = wallet->getGroup(group.type);
                     if (hdGroup->getLeaves().size() != group.leaves.size()) {
                        walletUpdated = true;
                        break;
                     }
                  }
               }
               if (!walletUpdated) {
                  return;
               }
               logger_->debug("[WalletsManager::onWalletsListUpdated] wallet {} has changed - resyncing"
                  , wallet->walletId());
               wallet->synchronize([this, wallet] {
                  wallet->registerWallet(armoryPtr_);
                  for (const auto &leaf : wallet->getLeaves()) {
                     if (!getWalletById(leaf->walletId())) {
                        logger_->debug("[WalletsManager::onWalletsListUpdated] adding new leaf {}"
                           , leaf->walletId());
                        addWallet(leaf, true);
                     }
                  }
                  wallet->scan([this, wallet](bs::sync::SyncState state) {
                     if (state == bs::sync::SyncState::Success) {
                        QMetaObject::invokeMethod(this, [this, wallet] {
                           emit walletChanged(wallet->walletId());
                        });
                     }
                  });
               });
            };
            signContainer_->syncHDWallet(wallet->walletId(), cbSyncHD);
         }
      }
      std::vector<std::string> hdWalletsId;
      hdWalletsId.reserve(hdWallets_.size());
      for (const auto &hdWallet : hdWallets_) {
         hdWalletsId.push_back(hdWallet->walletId());
      }
      for (const auto &hdWalletId : hdWalletsId) {
         if (hdWallets.find(hdWalletId) == hdWallets.end()) {
            deleteWallet(getHDWalletById(hdWalletId));
         }
      }
   };
   signContainer_->syncWalletInfo(cbSyncWallets);
}

void WalletsManager::onAuthLeafAdded(const std::string &walletId)
{
   if (walletId.empty()) {
      if (authAddressWallet_) {
         logger_->debug("[WalletsManager::onAuthLeafAdded] auth wallet {} unset", authAddressWallet_->walletId());
         deleteWallet(authAddressWallet_);
      }
      emit AuthLeafNotCreated();
      return;
   }
   const auto wallet = getPrimaryWallet();
   if (!wallet) {
      logger_->error("[WalletsManager::onAuthLeafAdded] no primary wallet loaded");
      emit AuthLeafNotCreated();
      return;
   }
   auto group = wallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   if (!group) {
      logger_->error("[WalletsManager::onAuthLeafAdded] no auth group in primary wallet");
      emit AuthLeafNotCreated();
      return;
   }

   const bs::hd::Path authPath({ bs::hd::Purpose::Native, bs::hd::CoinType::BlockSettle_Auth, 0 });
   logger_->debug("[WalletsManager::onAuthLeafAdded] creating auth leaf with id {}", walletId);
   auto leaf = group->getLeaf(authPath);
   if (leaf) {
      logger_->warn("[WalletsManager::onAuthLeafAdded] auth leaf already exists");
      group->deleteLeaf(authPath);
   }
   try {
      const bs::hd::Path authPath({ static_cast<bs::hd::Path::Elem>(bs::hd::Purpose::Native)
         , bs::hd::CoinType::BlockSettle_Auth, 0 });
      leaf = group->createLeaf(authPath, walletId);
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsManager::onAuthLeafAdded] failed to create auth leaf: {}", e.what());
      emit AuthLeafNotCreated();
      return;
   }
   leaf->synchronize([this, leaf] {
      logger_->debug("[WalletsManager::onAuthLeafAdded sync cb] Synchronized auth leaf has {} address[es]", leaf->getUsedAddressCount());
      addWallet(leaf, true);
      authAddressWallet_ = leaf;
      QMetaObject::invokeMethod(this, [this, walletId=leaf->walletId()] {
         emit AuthLeafCreated();
         emit authWalletChanged();
         emit walletChanged(walletId);
      });
   });
}

void WalletsManager::adoptNewWallet(const HDWalletPtr &wallet)
{
   saveWallet(wallet);
   if (armory_) {
      wallet->registerWallet(armoryPtr_);
   }
   emit newWalletAdded(wallet->walletId());
   emit walletsReady();
}

void WalletsManager::addWallet(const HDWalletPtr &wallet)
{
   if (!wallet) {
      return;
   }
   saveWallet(wallet);
   if (armory_) {
      wallet->registerWallet(armoryPtr_);
      emit walletsReady();
   }
}

bool WalletsManager::isWatchingOnly(const std::string &walletId) const
{
   if (signContainer_) {
      return signContainer_->isWalletOffline(walletId);
   }
   return false;
}

void WalletsManager::goOnline()
{
   for (const auto &cc : ccResolver_->securities()) {
      const auto tracker = std::make_shared<ColoredCoinTracker>(ccResolver_->lotSizeFor(cc)
         , armoryPtr_);
      tracker->addOriginAddress(ccResolver_->genesisAddrFor(cc));
      trackers_[cc] = tracker;
      logger_->debug("[{}] added CC tracker for {}", __func__, cc);
   }

   for (const auto &wallet : getAllWallets()) {
      auto ccLeaf = std::dynamic_pointer_cast<bs::sync::hd::CCLeaf>(wallet);
      if (ccLeaf) {
         updateTracker(ccLeaf);
      }
   }

   std::thread([this, handle = validityFlag_.handle(), trackers = trackers_, logger = logger_]() mutable {
      for (const auto &ccTracker : trackers) {
         if (!ccTracker.second->goOnline()) {
            logger->error("[WalletsManager::goOnline] failed for {}", ccTracker.first);
         }
      }

      ValidityGuard lock(handle);
      if (handle.isValid()) {
         QMetaObject::invokeMethod(this, &WalletsManager::walletsReady);
      }
   }).detach();
}

void WalletsManager::onCCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr)
{
   const auto &cc = ccProd.toStdString();
   logger_->debug("[{}] received info for {}", __func__, cc);
   const auto genAddr = bs::Address::fromAddressString(genesisAddr.toStdString());
   ccResolver_->addData(cc, nbSatoshis, genAddr, ccDesc.toStdString());
}

void WalletsManager::onCCInfoLoaded()
{
   logger_->debug("[WalletsManager::{}] - Re-validating against GAs in CC leaves"
      , __func__);
   for (const auto &wallet : wallets_) {
      if (wallet.second->type() != bs::core::wallet::Type::ColorCoin) {
         continue;
      }
      const auto ccWallet = std::dynamic_pointer_cast<bs::sync::hd::CCLeaf>(wallet.second);
      if (ccWallet) {
         ccWallet->setCCDataResolver(ccResolver_);
      }
      else {
         logger_->warn("[{}] invalid CC leaf {}", __func__, wallet.second->walletId());
      }
   }
   for (const auto &hdWallet : hdWallets_) {
      for (const auto &leaf : hdWallet->getLeaves()) {
         if (leaf->type() == bs::core::wallet::Type::ColorCoin) {
            leaf->init();
         }
      }
   }
}

// The initial point for processing an incoming zero conf TX. Important notes:
//
// - When getting the ZC list from Armory, previous ZCs won't clear out until
//   they have been confirmed.
// - If a TX has multiple instances of the same address, each instance will get
//   its own UTXO object while sharing the same UTXO hash.
// - It is possible, in conjunction with a wallet, to determine if the UTXO is
//   attached to an internal or external address.
void WalletsManager::onZCReceived(const std::vector<bs::TXEntry> &entries)
{
   std::vector<bs::TXEntry> ourZCentries;

   for (const auto &entry : entries) {
      for (const auto &walletId : entry.walletIds) {
         const auto wallet = getWalletById(walletId);
         if (!wallet) {
            continue;
         }
         logger_->debug("[WalletsManager::onZCReceived] - ZC entry in wallet {}"
            , wallet->name());

         // We have an affected wallet. Update it!
         ourZCentries.push_back(entry);
         //wallet->updateBalances();
         break;
      }
   } // for

     // Emit signals for the wallet and TX view models.
   QMetaObject::invokeMethod(this, [this] {emit blockchainEvent(); });
   if (!ourZCentries.empty()) {
      QMetaObject::invokeMethod(this, [this, ourZCentries] { emit newTransactions(ourZCentries); });
   }
}

void WalletsManager::onZCInvalidated(const std::set<BinaryData> &ids)
{
   QMetaObject::invokeMethod(this, [this, ids] {emit invalidatedZCs(ids); });
}

void WalletsManager::onTxBroadcastError(const std::string &txHash, const std::string &errMsg)
{
   logger_->error("[WalletsManager::{}] - TX {} error: {}", __func__, txHash, errMsg);
}

void WalletsManager::invokeFeeCallbacks(unsigned int blocks, float fee)
{
   std::vector<QObject *> objsToDelete;
   for (auto &cbByObj : feeCallbacks_) {
      const auto &it = cbByObj.second.find(blocks);
      if (it == cbByObj.second.end()) {
         continue;
      }
      if (!it->second.first) {
         break;
      }
      it->second.second(fee);
      cbByObj.second.erase(it);
      if (cbByObj.second.empty()) {
         objsToDelete.push_back(cbByObj.first);
      }
   }
   for (const auto &obj : objsToDelete) {
      feeCallbacks_.erase(obj);
   }
}

bool WalletsManager::estimatedFeePerByte(unsigned int blocksToWait, std::function<void(float)> cb, QObject *obj)
{
   if (!armory_) {
      return false;
   }
   auto blocks = blocksToWait;
   if (blocks < 2) {
      blocks = 2;
   } else if (blocks > 1008) {
      blocks = 1008;
   }

   if (lastFeePerByte_[blocks].isValid() && (lastFeePerByte_[blocks].secsTo(QDateTime::currentDateTime()) < 30)) {
      cb(feePerByte_[blocks]);
      return true;
   }

   bool callbackRegistered = false;
   for (const auto &cbByObj : feeCallbacks_) {
      if (cbByObj.second.find(blocks) != cbByObj.second.end()) {
         callbackRegistered = true;
         break;
      }
   }
   feeCallbacks_[obj][blocks] = { obj, cb };
   if (callbackRegistered) {
      return true;
   }
   const auto &cbFee = [this, blocks](float fee) {
      if (fee == std::numeric_limits<float>::infinity()) {
         invokeFeeCallbacks(blocks, fee);
         return;
      }
      fee = ArmoryConnection::toFeePerByte(fee);
      if (fee != 0) {
         feePerByte_[blocks] = fee;
         lastFeePerByte_[blocks] = QDateTime::currentDateTime();
         invokeFeeCallbacks(blocks, fee);
         return;
      }

      SPDLOG_LOGGER_WARN(logger_, "Fees estimation are not available, use hardcoded values!");
      if (blocks > 3) {
         feePerByte_[blocks] = 50;
      }
      else if (blocks >= 2) {
         feePerByte_[blocks] = 100;
      }
      invokeFeeCallbacks(blocks, feePerByte_[blocks]);
   };
   return armory_->estimateFee(blocks, cbFee);
}

bool WalletsManager::getFeeSchedule(const std::function<void(const std::map<unsigned int, float> &)> &cb)
{
   if (!armory_) {
      return false;
   }
   return armory_->getFeeSchedule(cb);
}

void WalletsManager::trackAddressChainUse(
   std::function<void(bool)> cb)
{
   /***
   This method grabs address txn count from the db for all managed
   wallets and deduces address chain use and type from the address
   tx counters.

   This is then reflected to the armory wallets through the
   SignContainer, to keep address chain counters and address types
   in sync.

   This method should be run only once per per, after registration.

   It will only have an effect if a wallet has been restored from
   seed or if there exist several instances of a wallet being used
   on different machines across time.

   More often than not, the armory wallet has all this meta data
   saved on disk to begin with.

   Callback is fired with either true (operation success) or
   false (SyncState_Failure, read below):

   trackChainAddressUse can return 3 states per wallet. These
   states are combined and processed as one when all wallets are
   done synchronizing. The states are as follow:

    - SyncState_Failure: the armory wallet failed to fine one or
      several of the addresses. This shouldn't typically happen.
      Most likely culprit is an address chain that is too short.
      Extend it.
      This state overrides all other states.

    - SyncState_NothingToDo: wallets are already sync'ed.
      Lowest priority.

    - SyncState_Success: Armory wallet address chain usage is now up
      to date, call WalletsManager::SyncWallets once again.
      Overrides NothingToDo.
   ***/

   auto ctr = std::make_shared<std::atomic<unsigned>>(0);
   auto wltCount = wallets_.size();
   auto state = std::make_shared<bs::sync::SyncState>(bs::sync::SyncState::NothingToDo);

   for (auto &it : wallets_)
   {
      auto trackLbd = [this, ctr, wltCount, state, cb](bs::sync::SyncState st)->void
      {
         switch (st)
         {
         case bs::sync::SyncState::Failure:
            *state = st;
            break;

         case bs::sync::SyncState::Success:
         {
            if (*state == bs::sync::SyncState::NothingToDo)
               *state = st;
            break;
         }

         default:
            break;
         }

         if (ctr->fetch_add(1) == wltCount - 1)
         {
            switch (*state)
            {
            case bs::sync::SyncState::Failure:
            {
               cb(false);
               return;
            }

            case bs::sync::SyncState::Success:
            {
               auto progLbd = [cb](int curr, int tot)->void
               {
                  if (curr == tot)
                     cb(true);
               };

               syncWallets(progLbd);
               return;
            }

            default:
               cb(true);
               return;
            }
         }
      };

      auto leafPtr = it.second;
      auto countLbd = [leafPtr, trackLbd](void)->void
      {
         leafPtr->trackChainAddressUse(trackLbd);
      };

      if (!leafPtr->getAddressTxnCounts(countLbd)) {
         cb(false);
      }
   }
}

void WalletsManager::addToMaintQueue(const MaintQueueCb &cb)
{
   std::unique_lock<std::mutex> lock(maintMutex_);
   maintQueue_.push_back(cb);
   maintCV_.notify_one();
}

void WalletsManager::maintenanceThreadFunc()
{
   while (maintThreadRunning_) {
      {
         std::unique_lock<std::mutex> lock(maintMutex_);
         if (maintQueue_.empty()) {
            maintCV_.wait_for(lock, std::chrono::milliseconds{ 500 });
         }
      }
      if (!maintThreadRunning_) {
         break;
      }
      decltype(maintQueue_) tempQueue;
      {
         std::unique_lock<std::mutex> lock(maintMutex_);
         tempQueue.swap(maintQueue_);
      }
      if (tempQueue.empty()) {
         continue;
      }

      for (const auto &cb : tempQueue) {
         if (!maintThreadRunning_) {
            break;
         }
         cb();
      }
   }
}


void WalletsManager::CCResolver::addData(const std::string &cc, uint64_t lotSize
   , const bs::Address &genAddr, const std::string &desc)
{
   securities_[cc] = { desc, lotSize, genAddr };
   const auto walletIdx = bs::hd::Path::keyToElem(cc);
   walletIdxMap_[walletIdx] = cc;
}

std::vector<std::string> WalletsManager::CCResolver::securities() const
{
   std::vector<std::string> result;
   for (const auto &ccDef : securities_) {
      result.push_back(ccDef.first);
   }
   return result;
}

std::string WalletsManager::CCResolver::nameByWalletIndex(bs::hd::Path::Elem idx) const
{
   idx &= ~bs::hd::hardFlag;
   const auto &itWallet = walletIdxMap_.find(idx);
   if (itWallet != walletIdxMap_.end()) {
      return itWallet->second;
   }
   return {};
}

uint64_t WalletsManager::CCResolver::lotSizeFor(const std::string &cc) const
{
   const auto &itSec = securities_.find(cc);
   if (itSec != securities_.end()) {
      return itSec->second.lotSize;
   }
   return 0;
}

std::string WalletsManager::CCResolver::descriptionFor(const std::string &cc) const
{
   const auto &itSec = securities_.find(cc);
   if (itSec != securities_.end()) {
      return itSec->second.desc;
   }
   return {};
}

bs::Address WalletsManager::CCResolver::genesisAddrFor(const std::string &cc) const
{
   const auto &itSec = securities_.find(cc);
   if (itSec != securities_.end()) {
      return itSec->second.genesisAddr;
   }
   return {};
}

bool WalletsManager::CreateCCLeaf(const std::string &ccName, const std::function<void(bs::error::ErrorCode result)> &cb)
{
   if (!isCCNameCorrect(ccName)) {
      logger_->error("[WalletsManager::CreateCCLeaf] invalid cc name passed: {}"
                     , ccName);
      return false;
   }

   // try to get cc leaf first, it might exist alread
   if (getCCWallet(ccName) != nullptr) {
      logger_->error("[WalletsManager::CreateCCLeaf] CC leaf already exists: {}"
                     , ccName);
      return false;
   }

   const auto primaryWallet = getPrimaryWallet();
   if (primaryWallet == nullptr) {
      logger_->error("[WalletsManager::CreateCCLeaf] there are no primary wallet. Could not create {}"
                     , ccName);
      return false;
   }

   bs::hd::Path path;

   path.append(static_cast<bs::hd::Path::Elem>(bs::hd::Purpose::Native) | bs::hd::hardFlag);
   path.append(bs::hd::BlockSettle_CC | bs::hd::hardFlag);
   path.append(ccName);

   bs::sync::PasswordDialogData dialogData;
   dialogData.setValue(PasswordDialogData::DialogType
      , ui::getPasswordInputDialogName(ui::PasswordInputDialogType::RequestPasswordForToken));
   dialogData.setValue(PasswordDialogData::Title, tr("Create CC Leaf"));
   dialogData.setValue(PasswordDialogData::Product, QString::fromStdString(ccName));

   const auto &createCCLeafCb = [this, ccName, cb](bs::error::ErrorCode result
      , const std::string &walletId) {
      processCreatedCCLeaf(ccName, result, walletId);
      if (cb) {
         cb(result);
      }
   };

   return signContainer_->createHDLeaf(primaryWallet->walletId(), path, {}, dialogData, createCCLeafCb);
}

void WalletsManager::processCreatedCCLeaf(const std::string &ccName, bs::error::ErrorCode result
   , const std::string &walletId)
{
   if (result == bs::error::ErrorCode::NoError) {
      logger_->debug("[WalletsManager::ProcessCreatedCCLeaf] CC leaf {} created with id {}"
                     , ccName, walletId);

      auto wallet = getPrimaryWallet();
      if (!wallet) {
         logger_->error("[WalletsManager::ProcessCreatedCCLeaf] primary wallet should exist");
         return;
      }

      auto group = wallet->getGroup(bs::hd::CoinType::BlockSettle_CC);
      if (!group) {
         logger_->error("[WalletsManager::ProcessCreatedCCLeaf] missing CC group");
         return;
      }

      const bs::hd::Path ccLeafPath({ bs::hd::Purpose::Native, bs::hd::CoinType::BlockSettle_CC
         , bs::hd::Path::keyToElem(ccName) });
      auto leaf = group->createLeaf(ccLeafPath, walletId);

      addWallet(leaf);
      newWallets_.insert(wallet->walletId());

      leaf->synchronize([this, leaf, ccName] {
         logger_->debug("CC leaf {} synchronized", ccName);
         leaf->registerWallet(armoryPtr_, true);

         emit CCLeafCreated(ccName);
      });
   } else {
      logger_->error("[WalletsManager::ProcessCreatedCCLeaf] CC leaf {} creation failed: {}"
                     , ccName, static_cast<int>(result));
      emit CCLeafCreateFailed(ccName, result);
   }
}

bool WalletsManager::PromoteHDWallet(const std::string& walletId
   , const std::function<void(bs::error::ErrorCode result)> &cb)
{
   const auto primaryWallet = getPrimaryWallet();
   if (primaryWallet != nullptr) {
      logger_->error("[WalletsManager::PromoteWallet] Primary wallet already exists {}"
                     , primaryWallet->walletId());
      return false;
   }

   bs::sync::PasswordDialogData dialogData;
   dialogData.setValue(PasswordDialogData::Title, tr("Promote To Primary Wallet"));
   dialogData.setValue(PasswordDialogData::XBT, tr("Authentification Addresses"));

   const auto& promoteHDWalletCb = [this, cb](bs::error::ErrorCode result
      , const std::string &walletId) {
      const auto wallet = getHDWalletById(walletId);
      if (!wallet) {
         logger_->error("[WalletsManager::PromoteWallet] failed to find wallet {}", walletId);
         if (cb) {
            cb(bs::error::ErrorCode::WalletNotFound);
         }
         return;
      }
      wallet->synchronize([this, cb, result, walletId] {
         processPromoteHDWallet(result, walletId);
         if (cb) {
            cb(result);
         }
      });
   };
   return signContainer_->promoteHDWallet(walletId, userId_, dialogData, promoteHDWalletCb);
}

void WalletsManager::processPromoteHDWallet(bs::error::ErrorCode result, const std::string& walletId)
{
   if (result == bs::error::ErrorCode::NoError) {
      auto const wallet = getHDWalletById(walletId);
      if (!wallet) {
         logger_->error("[WalletsManager::ProcessPromoteWalletID] wallet {} to promote does not exist"
            , walletId);
         return;
      }

      logger_->debug("[WalletsManager::ProcessPromoteWalletID] creating sync structure for wallet {}"
                     , walletId);
      wallet->createGroup(bs::hd::CoinType::BlockSettle_Auth, true);
      wallet->createGroup(bs::hd::CoinType::BlockSettle_Settlement, true);

      for (const auto &leaf : wallet->getLeaves()) {
         addWallet(leaf, true);
      }

      emit walletPromotedToPrimary(walletId);
      emit walletChanged(walletId);
   } else {
      logger_->error("[WalletsManager::ProcessPromoteWalletID] Wallet {} promotion failed: {}"
                     , walletId, static_cast<int>(result));
      emit walletPromotionFailed(walletId, result);
   }
}

void WalletsManager::updateTracker(const std::shared_ptr<hd::CCLeaf> &ccLeaf)
{
   const auto itTracker = trackers_.find(ccLeaf->displaySymbol().toStdString());
   if (itTracker != trackers_.end()) {
      ccLeaf->setCCTracker(itTracker->second);
   }
}

bool WalletsManager::createAuthLeaf(const std::function<void()> &cb)
{
   if (getAuthWallet() != nullptr) {
      logger_->error("[WalletsManager::CreateAuthLeaf] auth leaf already exists");
      return false;
   }

   if (userId_.isNull()) {
      logger_->error("[WalletsManager::CreateAuthLeaf] can't create auth leaf without user id");
      return false;
   }

   auto primaryWallet = getPrimaryWallet();
   if (primaryWallet == nullptr) {
      logger_->error("[WalletsManager::CreateAuthLeaf] could not create auth leaf - no primary wallet");
      return false;
   }

   const bs::hd::Path authPath({ bs::hd::Purpose::Native, bs::hd::CoinType::BlockSettle_Auth, 0 });
   bs::wallet::PasswordData pwdData;
   pwdData.salt = userId_;
   bs::sync::PasswordDialogData dialogData;
   dialogData.setValue(PasswordDialogData::DialogType
      , ui::getPasswordInputDialogName(ui::PasswordInputDialogType::RequestPasswordForAuthLeaf));
   dialogData.setValue(PasswordDialogData::Title, tr("Create Auth Leaf"));
   dialogData.setValue(PasswordDialogData::Product, QString::fromStdString(userId_.toHexStr()));

   const auto &createAuthLeafCb = [this, cb, primaryWallet, authPath]
      (bs::error::ErrorCode result, const std::string &walletId)
   {
      if (result != bs::error::ErrorCode::NoError) {
         logger_->error("[WalletsManager::createAuthLeaf] auth leaf creation failure: {}"
            , (int)result);
         emit AuthLeafNotCreated();
         return;
      }
      const auto group = primaryWallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
      const auto authGroup = std::dynamic_pointer_cast<bs::sync::hd::AuthGroup>(group);
      if (!authGroup) {
         logger_->error("[WalletsManager::createAuthLeaf] no auth group exists");
         emit AuthLeafNotCreated();
         return;
      }
      authGroup->setUserId(userId_);
      const auto leaf = authGroup->createLeaf(authPath, walletId);
      if (!leaf) {
         logger_->error("[WalletsManager::createAuthLeaf] failed to create auth leaf");
         emit AuthLeafNotCreated();
         return;
      }
      leaf->synchronize([this, cb, leaf] {
         leaf->registerWallet(armoryPtr_);
         authAddressWallet_ = leaf;
         addWallet(leaf, true);
         emit AuthLeafCreated();
         emit authWalletChanged();
         emit walletChanged(leaf->walletId());
         if (cb) {
            cb();
         }
      });
   };
   return signContainer_->createHDLeaf(primaryWallet->walletId(), authPath, { pwdData }
      , dialogData, createAuthLeafCb);
}

std::map<std::string, std::vector<bs::Address>> WalletsManager::getAddressToWalletsMapping(
   const std::vector<UTXO> &utxos) const
{
   std::map<std::string, std::vector<bs::Address>> result;
   for (const auto &utxo : utxos) {
      const auto addr = bs::Address::fromUTXO(utxo);
      const auto wallet = getWalletByAddress(addr);
      result[wallet ? wallet->walletId() : ""].push_back(addr);
   }
   return result;
}

std::shared_ptr<ResolverFeed> WalletsManager::getPublicResolver(const std::map<bs::Address, BinaryData> &piMap)
{
   return std::make_shared<bs::PublicResolver>(piMap);
}

bool WalletsManager::mergeableEntries(const bs::TXEntry &entry1, const bs::TXEntry &entry2) const
{
   if (entry1.txHash != entry2.txHash) {
      return false;
   }
   if (entry1.walletIds == entry2.walletIds) {
      return true;
   }
   WalletPtr wallet1;
   for (const auto &walletId : entry1.walletIds) {
      wallet1 = getWalletById(walletId);
      if (wallet1) {
         break;
      }
   }

   WalletPtr wallet2;
   for (const auto &walletId : entry2.walletIds) {
      wallet2 = getWalletById(walletId);
      if (wallet2) {
         break;
      }
   }

   if (!wallet1 || !wallet2) {
      return false;
   }
   if (wallet1 == wallet2) {
      return true;
   }

   if ((wallet1->type() == bs::core::wallet::Type::Bitcoin)
      && (wallet2->type() == wallet1->type())) {
      const auto rootWallet1 = getHDRootForLeaf(wallet1->walletId());
      const auto rootWallet2 = getHDRootForLeaf(wallet2->walletId());
      if (rootWallet1 == rootWallet2) {
         return true;
      }
   }
   return false;
}

std::vector<bs::TXEntry> WalletsManager::mergeEntries(const std::vector<bs::TXEntry> &entries) const
{
   std::vector<bs::TXEntry> mergedEntries;
   mergedEntries.reserve(entries.size());
   for (const auto &entry : entries) {
      if (mergedEntries.empty()) {
         mergedEntries.push_back(entry);
         continue;
      }
      bool entryMerged = false;
      for (auto &mergedEntry : mergedEntries) {
         if (mergeableEntries(mergedEntry, entry)) {
            entryMerged = true;
            mergedEntry.merge(entry);
            break;
         }
      }
      if (!entryMerged) {
         mergedEntries.push_back(entry);
      }
   }
   return mergedEntries;
}

bs::core::wallet::TXSignRequest WalletsManager::createPartialTXRequest(uint64_t spendVal
   , const std::map<UTXO, std::string> &inputs, bs::Address changeAddress
   , float feePerByte
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients
   , const bs::core::wallet::OutputSortOrder &outSortOrder
   , const BinaryData prevPart, bool feeCalcUsePrevPart)
{
   if (inputs.empty()) {
      throw std::invalid_argument("No usable UTXOs");
   }
   uint64_t fee = 0;
   uint64_t spendableVal = 0;
   std::vector<UTXO> utxos;
   utxos.reserve(inputs.size());
   for (const auto &input : inputs) {
      utxos.push_back(input.first);
      spendableVal += input.first.getValue();
   }

   if (feePerByte > 0) {
      unsigned int idMap = 0;
      std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipMap;
      for (const auto &recip : recipients) {
         if (recip->getValue()) {
            recipMap.emplace(idMap++, recip);
         }
      }

      PaymentStruct payment(recipMap, 0, feePerByte, ADJUST_FEE);
      for (auto &utxo : utxos) {
         const auto scrAddr = bs::Address::fromHash(utxo.getRecipientScrAddr());
         utxo.txinRedeemSizeBytes_ = (unsigned int)scrAddr.getInputSize();
         utxo.witnessDataSizeBytes_ = unsigned(scrAddr.getWitnessDataSize());
         utxo.isInputSW_ = (scrAddr.getWitnessDataSize() != UINT32_MAX);
      }

      const auto coinSelection = std::make_shared<CoinSelection>([utxos](uint64_t) { return utxos; }
         , std::vector<AddressBookEntry>{}, spendableVal
         , armory_ ? armory_->topBlock() : UINT32_MAX);

      try {
         const auto selection = coinSelection->getUtxoSelectionForRecipients(payment, utxos);
         fee = selection.fee_;
         utxos = selection.utxoVec_;
      } catch (const std::exception &e) {
         SPDLOG_LOGGER_ERROR(logger_, "coin selection failed: {}, all inputs will be used", e.what());
      }
   }
   /*   else {    // use all supplied inputs
         size_t nbUtxos = 0;
         for (auto &utxo : utxos) {
            inputAmount += utxo.getValue();
            nbUtxos++;
            if (inputAmount >= (spendVal + fee)) {
               break;
            }
         }
         if (nbUtxos < utxos.size()) {
            utxos.erase(utxos.begin() + nbUtxos, utxos.end());
         }
      }*/

   if (utxos.empty()) {
      throw std::logic_error("No UTXOs");
   }

   std::set<std::string> walletIds;
   for (const auto &utxo : utxos) {
      const auto &itInput = inputs.find(utxo);
      if (itInput == inputs.end()) {
         continue;
      }
      walletIds.insert(itInput->second);
   }
   if (walletIds.empty()) {
      throw std::logic_error("No wallet IDs");
   }

   bs::core::wallet::TXSignRequest request;
   request.walletIds.insert(request.walletIds.end(), walletIds.cbegin(), walletIds.cend());
   request.populateUTXOs = true;
   request.outSortOrder = outSortOrder;
   Signer signer;
   bs::CheckRecipSigner prevStateSigner;
   if (!prevPart.isNull()) {
      prevStateSigner.deserializeState(prevPart);
      if (feePerByte > 0) {
         fee += prevStateSigner.estimateFee(feePerByte);
         fee -= 10 * feePerByte;    // subtract TX header size as it's counted twice
      }
      for (const auto &spender : prevStateSigner.spenders()) {
         signer.addSpender(spender);
      }
   }
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);
   request.fee = fee;

   uint64_t inputAmount = 0;
   if (feeCalcUsePrevPart) {
      for (const auto &spender : prevStateSigner.spenders()) {
         inputAmount += spender->getValue();
      }
   }
   for (const auto &utxo : utxos) {
      signer.addSpender(std::make_shared<ScriptSpender>(utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      request.inputs.push_back(utxo);
      inputAmount += utxo.getValue();
      /*      if (inputAmount >= (spendVal + fee)) {
               break;
            }*/   // use all provided inputs now (will be uncommented if some logic depends on it)
   }
   if (!inputAmount) {
      throw std::logic_error("No inputs detected");
   }

   const auto addRecipients = [&request, &signer]
   (const std::vector<std::shared_ptr<ScriptRecipient>> &recipients)
   {
      for (const auto& recipient : recipients) {
         request.recipients.push_back(recipient);
         signer.addRecipient(recipient);
      }
   };

   if (inputAmount < (spendVal + fee)) {
      throw std::overflow_error("Not enough inputs (" + std::to_string(inputAmount)
         + ") to spend " + std::to_string(spendVal + fee));
   }

   for (const auto &outputType : outSortOrder) {
      switch (outputType) {
      case bs::core::wallet::OutputOrderType::Recipients:
         addRecipients(recipients);
         break;
      case bs::core::wallet::OutputOrderType::PrevState:
         addRecipients(prevStateSigner.recipients());
         break;
      case bs::core::wallet::OutputOrderType::Change:
         if (inputAmount == (spendVal + fee)) {
            break;
         }
         {
            const uint64_t changeVal = inputAmount - (spendVal + fee);
            if (changeAddress.isNull()) {
               throw std::invalid_argument("Change address required, but missing");
            }
            signer.addRecipient(changeAddress.getRecipient(bs::XBTAmount{ changeVal }));
            request.change.value = changeVal;
            request.change.address = changeAddress;
         }
         break;
      default:
         throw std::invalid_argument("Unsupported output type " + std::to_string((int)outputType));
      }
   }

   request.prevStates.emplace_back(signer.serializeState());
   return request;
}
