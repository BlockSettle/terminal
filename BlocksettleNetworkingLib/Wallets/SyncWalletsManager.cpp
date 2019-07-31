#include "SyncWalletsManager.h"

#include "ApplicationSettings.h"
#include "FastLock.h"
#include "SyncHDWallet.h"
#include "SyncSettlementWallet.h"
#include "WalletSignerContainer.h"

#include <QCoreApplication>
#include <QDir>
#include <QMutexLocker>

#include <spdlog/spdlog.h>

using namespace bs::sync;

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
   : QObject(nullptr), ArmoryCallbackTarget(armory.get())
   , logger_(logger)
   , appSettings_(appSettings)
   , armoryPtr_(armory)
{
   ccResolver_ = std::make_shared<CCResolver>();
   maintThreadRunning_ = true;
   maintThread_ = std::thread(&WalletsManager::maintenanceThreadFunc, this);
}

WalletsManager::~WalletsManager() noexcept
{
   for (const auto &hdWallet : hdWallets_) {
      hdWallet.second->setWCT(nullptr);
   }
   {
      std::unique_lock<std::mutex> lock(maintMutex_);
      maintThreadRunning_ = false;
      maintCV_.notify_one();
   }
   if (maintThread_.joinable()) {
      maintThread_.join();
   }
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
   walletsId_.clear();
   hdWalletsId_.clear();
   authAddressWallet_.reset();

   emit walletChanged("");
}

void WalletsManager::syncWallets(const CbProgress &cb)
{
   const auto &cbWalletInfo = [this, cb](const std::vector<bs::sync::WalletInfo> &wi) {
      auto walletIds = std::make_shared<std::unordered_set<std::string>>();
      for (const auto &info : wi)
         walletIds->insert(info.id);

      for (const auto &info : wi) {
         const auto &cbDone = [this, walletIds, id=info.id, total=wi.size(), cb]
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

         logger_->debug("[WalletsManager::syncWallets] syncing wallet {} ({} {})"
            , info.id, info.name, (int)info.format);
         
         switch (info.format) {
         case bs::sync::WalletFormat::HD:
         {
            try {
               const auto hdWallet = std::make_shared<hd::Wallet>(info.id, info.name,
                  info.description, signContainer_.get(), logger_);
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
            }
            catch (const std::exception &e) {
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
   }
   wallet->setUserId(userId_);

   QMutexLocker lock(&mtxWallets_);
   auto insertIter = walletsId_.insert(wallet->walletId());
   if (!insertIter.second) {
      auto wltIter = wallets_.find(wallet->walletId());
      if (wltIter == wallets_.end()) {
         throw std::runtime_error("have id but lack leaf ptr");
      }
   } else {
      wallets_[wallet->walletId()] = wallet;
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
   if (!userId_.isNull())
      wallet->setUserId(userId_);

   auto insertIter = hdWalletsId_.insert(wallet->walletId());

   //integer id signifying the wallet's insertion order
   if (!insertIter.second)
   {
      //wallet already exist in container, merge content instead
      auto wltIter = hdWallets_.find(wallet->walletId());
      if (wltIter == hdWallets_.end())
         throw std::runtime_error("have wallet id but no ptr");

      wltIter->second->merge(*wallet);
   }

   //map::insert will not replace the wallet
   wallet->containerId_ = hdWallets_.size();
   hdWallets_.insert(make_pair(wallet->walletId(), wallet));
   walletNames_.insert(wallet->name());

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
         const auto leaf = hdWallet.second->getLeaf(walletId);
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
      if (wallet.second->isPrimary()) {
         return wallet.second;
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
      result = group ? group->getLeaf(bs::hd::hardFlag) : nullptr;
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
   return ccGroup->getLeaf(cc);
}

void WalletsManager::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   for (const auto &hdWallet : hdWallets_) {
      hdWallet.second->setUserId(userId);
   }
   auto primaryWallet = getPrimaryWallet();
   if (primaryWallet) {
      signContainer_->setUserId(userId, primaryWallet->walletId());
   }
}

const WalletsManager::HDWalletPtr WalletsManager::getHDWallet(unsigned id) const
{
   for (auto& wltPair : hdWallets_)
   {
      if (wltPair.second->containerId_ == id)
         return wltPair.second;
   }

   throw std::runtime_error("unknown wallet int id");
   return nullptr;
}

const WalletsManager::HDWalletPtr WalletsManager::getHDWalletById(const std::string& walletId) const
{
   auto it = hdWallets_.find(walletId);
   if (it != hdWallets_.end()) {
      return it->second;
   }
   return nullptr;
}

const WalletsManager::HDWalletPtr WalletsManager::getHDRootForLeaf(const std::string& walletId) const
{
   for (const auto &hdWallet : hdWallets_) {
      if (hdWallet.second->getLeaf(walletId)) {
         return hdWallet.second;
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
   for (const auto wallet : wallets_) {
      if (wallet.second && (wallet.second->containsAddress(address)
         || wallet.second->containsHiddenAddress(address))) {
         return wallet.second;
      }
   }
   return nullptr;
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

void WalletsManager::onNewBlock(unsigned int)
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
   if (!armory_) {
      return;
   }
   const auto rootWallet = getHDRootForLeaf(walletId);
   if (rootWallet) {
      const auto &itWallet = newWallets_.find(rootWallet->walletId());
      if (itWallet != newWallets_.end()) {
         rootWallet->startRescan();
         newWallets_.erase(itWallet);
         rootWallet->synchronize([this, walletId] {
            QMetaObject::invokeMethod(this, [this, walletId] {
               emit walletChanged(walletId);
            });
         });
      }
   }

   readyWallets_.insert(walletId);
   auto nbWallets = wallets_.size();
   if (readyWallets_.size() >= nbWallets) {
      isReady_ = true;
      logger_->debug("[WalletsManager::{}] - All wallets are ready", __func__);
      emit walletsReady();
      readyWallets_.clear();
   }
}

void WalletsManager::scanComplete(const std::string &walletId)
{
   logger_->debug("[WalletsManager::{}] - HD wallet {} imported", __func__
      , walletId);
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
   const auto itId = std::find(walletsId_.begin(), walletsId_.end(), wallet->walletId());
   if (itId != walletsId_.end()) {
      walletsId_.erase(itId);
   }
   wallets_.erase(wallet->walletId());
}

bool WalletsManager::deleteWallet(WalletPtr wallet)
{
   bool isHDLeaf = false;
   logger_->info("[WalletsManager::{}] - Removing wallet {} ({})...", __func__
      , wallet->name(), wallet->walletId());
   for (auto hdWallet : hdWallets_) {
      const auto leaves = hdWallet.second->getLeaves();
      if (std::find(leaves.begin(), leaves.end(), wallet) != leaves.end()) {
         for (auto group : hdWallet.second->getGroups()) {
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
   const auto it = hdWallets_.find(wallet->walletId());
   if (it == hdWallets_.end()) {
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

   const auto itId = std::find(hdWalletsId_.begin(), hdWalletsId_.end(), wallet->walletId());
   if (itId != hdWalletsId_.end()) {
      hdWalletsId_.erase(itId);
   }
   hdWallets_.erase(wallet->walletId());
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
      if (ids.empty()) {
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
      logger_->error("[WalletsManager::{}] - TX not initialized", __func__);
      return false;
   }

   if (!armory_) {
      logger_->error("[WalletsManager::{}] - armory not set", __func__);
      return false;
   }

   const auto wallet = getWalletById(walletId);
   if (!wallet) {
      logger_->error("[WalletsManager::{}] - failed to get wallet for id {}"
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

   const auto &cbProcess = [this, wallet, tx, txKey, txOutIndices, cb](const std::vector<Tx> &txs) {
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
            const auto &addrWallet = getWalletByAddress(addr);
            ((addrWallet == wallet) ? ourIns : otherIns) = true;
            if (addrWallet && (addrWallet->type() == bs::core::wallet::Type::ColorCoin)) {
               ccTx = true;
            }
            txOuts.emplace_back(prevOut);
            inAddrs.emplace_back(std::move(addr));
         }
      }

      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy((int)i);
         const auto addrWallet = getWalletByAddress(out.getScrAddressStr());
         ((addrWallet == wallet) ? ourOuts : otherOuts) = true;
         if (addrWallet && (addrWallet->type() == bs::core::wallet::Type::ColorCoin)) {
            ccTx = true;
            break;
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
   std::set<bs::Address> addresses;
   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy((int)i);
      const auto addr = bs::Address::fromTxOut(out);
      bool isOurs = (getWalletByAddress(addr) == wallet);
      if ((isOurs == isReceiving) || (isOurs && isSettlement)) {
         addresses.insert(addr);
      }
   }

   const auto &cbProcessAddresses = [this, txKey, cb](const std::set<bs::Address> &addresses) {
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

   if (addresses.empty()) {
      std::set<BinaryData> opTxHashes;
      std::map<BinaryData, std::vector<uint32_t>> txOutIndices;

      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         TxIn in = tx.getTxInCopy((int)i);
         OutPoint op = in.getOutPoint();

         opTxHashes.insert(op.getTxHash());
         txOutIndices[op.getTxHash()].push_back(op.getTxOutIndex());
      }

      const auto &cbProcess = [this, txOutIndices, wallet, cbProcessAddresses](const std::vector<Tx> &txs) {
         std::set<bs::Address> addresses;
         for (const auto &prevTx : txs) {
            const auto &itIdx = txOutIndices.find(prevTx.getThisHash());
            if (itIdx == txOutIndices.end()) {
               continue;
            }
            for (const auto idx : itIdx->second) {
               const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy((int)idx));
               if (getWalletByAddress(addr) == wallet) {
                  addresses.insert(addr);
               }
            }
         }
         cbProcessAddresses(addresses);
      };
      if (opTxHashes.empty()) {
         logger_->error("[WalletsManager::{}] - empty TX hashes", __func__);
         return false;
      }
      else {
         armory_->getTXsByHash(opTxHashes, cbProcess);
      }
   }
   else {
      cbProcessAddresses(addresses);
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
      logger_->error("[WalletsManager::{}] - signer is not set - aborting"
         , __func__);
      if (cb)
         cb({});
      return;
   }
   signContainer_->createSettlementWallet(authAddr, cb);
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
   std::set<std::string> hdWalletIds;
   hdWalletIds.insert(hdWalletsId_.cbegin(), hdWalletsId_.cend());
   const auto &cbSyncWallets = [this, hdWalletIds](int cur, int total) {
      if (cur < total) {
         return;
      }
      for (const auto &hdWalletId : hdWalletsId_) {
         if (hdWalletIds.find(hdWalletId) == hdWalletIds.end()) {
            const auto hdWallet = hdWallets_[hdWalletId];
            QMetaObject::invokeMethod(this, [this, hdWallet] { emit walletAdded(hdWallet->walletId()); });
            logger_->debug("[WalletsManager::onWalletsListUpdated] found new wallet {} "
               "- starting address scan for it", hdWalletId);
            newWallets_.insert(hdWalletId);
         }
      }
      for (const auto &hdWalletId : hdWalletIds) {
         if (std::find(hdWalletsId_.cbegin(), hdWalletsId_.cend(), hdWalletId) == hdWalletsId_.cend()) {
            QMetaObject::invokeMethod(this, [this, hdWalletId] { emit walletDeleted(hdWalletId); });
         }
      }
      registerWallets();
   };
   reset();
   syncWallets(cbSyncWallets);
}

void WalletsManager::onAuthLeafAdded(const std::string &walletId)
{
   if (walletId.empty()) {
      if (authAddressWallet_) {
         logger_->debug("[WalletsManager::onAuthLeafAdded] auth wallet {} unset", authAddressWallet_->walletId());
         deleteWallet(authAddressWallet_);
      }
      return;
   }
   const auto wallet = getPrimaryWallet();
   if (!wallet) {
      logger_->error("[WalletsManager::onAuthLeafAdded] no primary wallet loaded");
      return;
   }
   auto group = wallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   if (!group) {
      logger_->error("[WalletsManager::onAuthLeafAdded] no auth group in primary wallet");
      return;
   }

   logger_->debug("[WalletsManager::onAuthLeafAdded] creating auth leaf with id {}", walletId);
   auto leaf = group->getLeaf(0 | bs::hd::hardFlag);
   if (leaf) {
      logger_->warn("[WalletsManager::onAuthLeafAdded] auth leaf already exists");
      group->deleteLeaf(0 | bs::hd::hardFlag);
   }
   try {
      leaf = group->createLeaf(bs::hd::hardFlag, walletId);
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsManager::onAuthLeafAdded] failed to create auth leaf: {}", e.what());
      return;
   }
   leaf->synchronize([this, leaf] {
      logger_->debug("[WalletsManager::onAuthLeafAdded sync cb] Synchronized auth leaf has {} address[es]", leaf->getUsedAddressCount());
      addWallet(leaf, true);
      authAddressWallet_ = leaf;
      authAddressWallet_->registerWallet(armoryPtr_);
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

void WalletsManager::onCCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr)
{
   const auto &cc = ccProd.toStdString();
   ccResolver_->addData(cc, nbSatoshis, genesisAddr.toStdString(), ccDesc.toStdString());
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
      for (const auto &leaf : hdWallet.second->getLeaves()) {
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
      auto wallet = getWalletById(entry.walletId);
      if (wallet != nullptr) {
         logger_->debug("[WalletsManager::{}] - ZC entry in wallet {}", __func__
            , wallet->name());

         // We have an affected wallet. Update it!
         ourZCentries.push_back(entry);
         //wallet->updateBalances();
      } // if
      else {
         logger_->debug("[WalletsManager::{}] - get ZC but wallet not found: {}"
            , __func__, entry.walletId);
      }
   } // for

     // Emit signals for the wallet and TX view models.
   QMetaObject::invokeMethod(this, [this] {emit blockchainEvent(); });
   if (!ourZCentries.empty()) {
      QMetaObject::invokeMethod(this, [this, ourZCentries] { emit newTransactions(ourZCentries); });
   }
}

void WalletsManager::onZCInvalidated(const std::vector<bs::TXEntry> &entries)
{
   if (!entries.empty()) {
      QMetaObject::invokeMethod(this, [this, entries] {emit invalidatedZCs(entries); });
   }
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
      fee *= BTCNumericTypes::BalanceDivider / 1000.0;
      if (fee != 0) {
         if (fee < 5) {
            fee = 5;
         }
         feePerByte_[blocks] = fee;
         lastFeePerByte_[blocks] = QDateTime::currentDateTime();
         invokeFeeCallbacks(blocks, fee);
         return;
      }

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

// virtual bool createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
//       , const std::vector<bs::wallet::PasswordData> &pwdData = {}
//       , const std::function<void(bs::error::ErrorCode result)> &cb = nullptr) = 0;

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

   path.append(bs::hd::purpose | bs::hd::hardFlag);
   path.append(bs::hd::BlockSettle_CC | bs::hd::hardFlag);
   path.append(ccName);

   bs::sync::PasswordDialogData dialogData;
   dialogData.setValue("Title", tr("Create CC Leaf"));
   dialogData.setValue("Product", QString::fromStdString(ccName));

   const auto &createCCLeafCb = [this, ccName, cb](bs::error::ErrorCode result) {
      ProcessCreatedCCLeaf(ccName, result);
      if (cb) {
         cb(result);
      }
   };

   return signContainer_->createHDLeaf(primaryWallet->walletId(), path, {}, dialogData, createCCLeafCb);
}

void WalletsManager::ProcessCreatedCCLeaf(const std::string &ccName, bs::error::ErrorCode result)
{
   if (result == bs::error::ErrorCode::NoError) {
      logger_->debug("[WalletsManager::ProcessCreatedCCLeaf] CC leaf {} created"
                     , ccName);

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

      auto leaf = group->createLeaf(ccName, wallet->walletId());

      addWallet(leaf);

      // XXX register in armory ?
      // XXX rescan ?

      emit CCLeafCreated(ccName);
      emit walletChanged(wallet->walletId());
   } else {
      logger_->error("[WalletsManager::ProcessCreatedCCLeaf] CC leaf {} creation failed: {}"
                     , ccName, static_cast<int>(result));
      emit CCLeafCreateFailed(ccName, result);
   }
}

bool WalletsManager::CreateAuthLeaf()
{
   if (getAuthWallet() != nullptr) {
      logger_->error("[WalletsManager::CreateAuthLeaf] auth leaf already exists");
      return false;
   }

   auto primaryWallet = getPrimaryWallet();
   if (primaryWallet == nullptr) {
      logger_->error("[WalletsManager::CreateAuthLeaf] could not create auth leaf. no primary wallet");
      return false;
   }

   bs::hd::Path path;
   path.append(bs::hd::purpose | bs::hd::hardFlag);
   path.append(bs::hd::CoinType::BlockSettle_Auth | bs::hd::hardFlag);
   path.append(0 | bs::hd::hardFlag);

   return signContainer_->createHDLeaf(primaryWallet->walletId(), path, {}, {},  [this](bs::error::ErrorCode result)
                                       {
                                          ProcessAuthLeafCreateResult(result);
                                       });
}

void WalletsManager::ProcessAuthLeafCreateResult(bs::error::ErrorCode result)
{
   logger_->debug("[WalletsManager::ProcessAuthLeafCreateResult] auth leaf creation result: {}"
                  , static_cast<int>(result));
   // No need to react on positive result, since WalletSignerContainer::AuthLeafAdded should be emitted
}
