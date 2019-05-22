#include "SyncWalletsManager.h"

#include "ApplicationSettings.h"
#include "FastLock.h"
#include "SignContainer.h"
#include "SyncHDWallet.h"
#include "SyncSettlementWallet.h"

#include <QCoreApplication>
#include <QDir>
#include <QMutexLocker>

#include <spdlog/spdlog.h>

using namespace bs::sync;

WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ApplicationSettings>& appSettings, const std::shared_ptr<ArmoryObject> &armory)
   : QObject(nullptr)
   , logger_(logger)
   , appSettings_(appSettings)
   , armory_(armory)
{
   if (armory_) {
      connect(armory_.get(), &ArmoryObject::zeroConfReceived, this, &WalletsManager::onZeroConfReceived, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryObject::zeroConfInvalidated, this, &WalletsManager::onZeroConfInvalidated, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryObject::txBroadcastError, this, &WalletsManager::onBroadcastZCError, Qt::QueuedConnection);
      connect(armory_.get(), SIGNAL(stateChanged(ArmoryConnection::State)), this, SLOT(onStateChanged(ArmoryConnection::State)), Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryObject::newBlock, this, &WalletsManager::onNewBlock, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryObject::refresh, this, &WalletsManager::onRefresh, Qt::QueuedConnection);
   }
}

void WalletsManager::setSignContainer(const std::shared_ptr<SignContainer> &container)
{
   signContainer_ = container;

   connect(signContainer_.get(), &SignContainer::HDWalletCreated, this, &WalletsManager::onHDWalletCreated);
   connect(signContainer_.get(), &SignContainer::walletsListUpdated, this, &WalletsManager::onWalletsListUpdated);
}

WalletsManager::~WalletsManager() noexcept = default;

void WalletsManager::reset()
{
   wallets_.clear();
   hdWallets_.clear();
   hdDummyWallet_.reset();
   walletNames_.clear();
   readyWallets_.clear();
   walletsId_.clear();
   hdWalletsId_.clear();
   settlementWallet_.reset();
   authAddressWallet_.reset();

   emit walletChanged();
}

void WalletsManager::syncWallets(const CbProgress &cb)
{
   const auto &cbWalletInfo = [this, cb](const std::vector<bs::sync::WalletInfo> &wi) {
      auto walletIds = std::make_shared<std::unordered_set<std::string>>();
      for (const auto &info : wi) {
         walletIds->insert(info.id);
      }
      for (const auto &info : wi) {
         const auto &cbDone = [this, walletIds, id=info.id, total=wi.size(), cb] {
            walletIds->erase(id);
            if (cb) {
               cb(total - walletIds->size(), total);
            }
            if (walletIds->empty()) {
               logger_->debug("[WalletsManager::syncWallets] all wallets synchronized");
               emit walletsSynchronized();
               emit walletChanged();
            }
         };

         logger_->debug("[WalletsManager::syncWallets] syncing wallet {} ({} {})"
            , info.id, info.name, (int)info.format);
         switch (info.format) {
         case bs::sync::WalletFormat::HD: {
            try {
               const auto hdWallet = std::make_shared<hd::Wallet>(info.netType, info.id, info.name
                  , info.description, signContainer_.get(), logger_);
               if (hdWallet) {
                  const auto &cbHDWalletDone = [this, hdWallet, cbDone] {
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
         case bs::sync::WalletFormat::Settlement: {
            if (settlementWallet_) {
               logger_->error("[WalletsManager::syncWallets] more than one settlement "
                  "wallet is not supported");
               cbDone();
            }
            else {
               const auto settlWallet = std::make_shared<SettlementWallet>(info.id, info.name, info.description
                  , signContainer_.get(), logger_);
               const auto &cbSettlementDone = [this, settlWallet, cbDone] {
                  setSettlementWallet(settlWallet);
                  cbDone();
               };
               settlWallet->synchronize(cbSettlementDone);
            }
            break;
         }
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
         emit walletsSynchronized();
      }
   };
   if (!signContainer_) {
      logger_->error("[WalletsManager::{}] signer is not set - aborting"
         , __func__);
      return;
   }
   signContainer_->syncWalletInfo(cbWalletInfo);
}

bool WalletsManager::isReadyForTrading() const
{
   return (hasPrimaryWallet() && hasSettlementWallet());
}

void WalletsManager::registerSettlementWallet()
{
   if (!settlementWallet_) {
      return;
   }
   connect(settlementWallet_.get(), &SettlementWallet::walletReady, this, &WalletsManager::onWalletReady);
   if (armory_) {
      settlementWallet_->registerWallet(armory_);
   }
}

void WalletsManager::saveWallet(const WalletPtr &newWallet)
{
   if (hdDummyWallet_ == nullptr) {
      hdDummyWallet_ = std::make_shared<hd::DummyWallet>(logger_);
      hdWalletsId_.emplace_back(hdDummyWallet_->walletId());
      hdWallets_[hdDummyWallet_->walletId()] = hdDummyWallet_;
   }
   addWallet(newWallet);
}

void WalletsManager::addWallet(const WalletPtr &wallet, bool isHDLeaf)
{
   if (!isHDLeaf && hdDummyWallet_) {
      hdDummyWallet_->add(wallet);
   }
   {
      QMutexLocker lock(&mtxWallets_);
      walletsId_.emplace_back(wallet->walletId());
      wallets_.emplace(wallet->walletId(), wallet);
   }
   connect(wallet.get(), &Wallet::walletReady, this, &WalletsManager::onWalletReady);
   connect(wallet.get(), &Wallet::addressAdded, [this] { emit walletChanged(); });
   connect(wallet.get(), &Wallet::walletReset, [this] { emit walletChanged(); });
   connect(wallet.get(), &Wallet::balanceUpdated, [this](std::string walletId, std::vector<uint64_t>) {
      emit walletBalanceUpdated(walletId); });
   connect(wallet.get(), &Wallet::balanceChanged, [this](std::string walletId, std::vector<uint64_t>) {
      emit walletBalanceChanged(walletId); });
}

void WalletsManager::saveWallet(const HDWalletPtr &wallet)
{
   if (!userId_.isNull()) {
      wallet->setUserId(userId_);
   }
   hdWalletsId_.emplace_back(wallet->walletId());
   hdWallets_[wallet->walletId()] = wallet;
   walletNames_.insert(wallet->name());
   for (const auto &leaf : wallet->getLeaves()) {
      addWallet(leaf, true);
   }
   connect(wallet.get(), &hd::Wallet::leafAdded, this, &WalletsManager::onHDLeafAdded);
   connect(wallet.get(), &hd::Wallet::leafDeleted, this, &WalletsManager::onHDLeafDeleted);
   connect(wallet.get(), &hd::Wallet::scanComplete, this, &WalletsManager::onWalletImported, Qt::QueuedConnection);
}

void WalletsManager::setSettlementWallet(const std::shared_ptr<bs::sync::SettlementWallet> &wallet)
{
   settlementWallet_ = wallet;
   connect(wallet.get(), &Wallet::walletReady, this, &WalletsManager::onWalletReady);
   connect(wallet.get(), &Wallet::addressAdded, [this] { emit walletChanged(); });
   connect(wallet.get(), &Wallet::walletReset, [this] { emit walletChanged(); });
   connect(wallet.get(), &Wallet::balanceUpdated, [this](std::string walletId, std::vector<uint64_t>) {
      emit walletBalanceUpdated(walletId); });
   connect(wallet.get(), &Wallet::balanceChanged, [this](std::string walletId, std::vector<uint64_t>) {
      emit walletBalanceChanged(walletId); });
}

bool WalletsManager::setAuthWalletFrom(const HDWalletPtr &wallet)
{
   const auto group = wallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   if ((group != nullptr) && (group->getNumLeaves() > 0)) {
      authAddressWallet_ = group->getLeaf(0);
      return true;
   }
   return false;
}

void WalletsManager::onHDLeafAdded(QString id)
{
   for (const auto &hdWallet : hdWallets_) {
      const auto &leaf = hdWallet.second->getLeaf(id.toStdString());
      if (leaf == nullptr) {
         continue;
      }
      logger_->debug("[WalletsManager::{}] HD leaf {} ({}) added", __func__
         , id.toStdString(), leaf->name());

      const auto &ccIt = ccSecurities_.find(leaf->shortName());
      if (ccIt != ccSecurities_.end()) {
         leaf->setDescription(ccIt->second.desc);
         leaf->setData(ccIt->second.genesisAddr);
         leaf->setData(ccIt->second.lotSize);
      }

      leaf->setUserId(userId_);
      addWallet(leaf);
      if (armory_) {
         leaf->registerWallet(armory_);
      }

      if (setAuthWalletFrom(hdWallet.second)) {
         logger_->debug("[WalletsManager::{}] - Auth wallet updated", __func__);
         emit authWalletChanged();
      }
      emit walletChanged();
      break;
   }
}

void WalletsManager::onHDLeafDeleted(QString id)
{
   const auto &wallet = getWalletById(id.toStdString());
   eraseWallet(wallet);
   emit walletChanged();
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
      result = group ? group->getLeaf(0) : nullptr;
   }
   return result;
}

WalletsManager::WalletPtr WalletsManager::getCCWallet(const std::string &cc)
{
   if (cc.empty() || !hasPrimaryWallet()) {
      return nullptr;
   }
   if ((cc.length() == 1) && (cc[0] >= '0') && (cc[0] <= '9')) {
      return nullptr;
   }
   const auto &priWallet = getPrimaryWallet();
   auto ccGroup = priWallet->getGroup(bs::hd::CoinType::BlockSettle_CC);
   if (ccGroup == nullptr) {
      ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   }
   return ccGroup->getLeaf(cc);
}

void WalletsManager::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   for (const auto &hdWallet : hdWallets_) {
      hdWallet.second->setUserId(userId);
   }
}

const WalletsManager::HDWalletPtr WalletsManager::getHDWallet(const unsigned int index) const
{
   if (index >= hdWalletsId_.size()) {
      return nullptr;
   }
   return getHDWalletById(hdWalletsId_[index]);
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

   if (settlementWallet_ && (settlementWallet_->walletId() == walletId)) {
      return settlementWallet_;
   }
   return nullptr;
}

WalletsManager::WalletPtr WalletsManager::getWalletByAddress(const bs::Address &addr) const
{
   const auto &address = addr.unprefixed();
   {
      for (const auto wallet : wallets_) {
         if (wallet.second && (wallet.second->containsAddress(address)
            || wallet.second->containsHiddenAddress(address))) {
            return wallet.second;
         }
      }
   }
   if ((settlementWallet_ != nullptr) && settlementWallet_->containsAddress(address)) {
      return settlementWallet_;
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
   return getBalanceSum([](WalletPtr wallet) { return wallet->getUnconfirmedBalance(); });
}

BTCNumericTypes::balance_type WalletsManager::getTotalBalance() const
{
   return getBalanceSum([](WalletPtr wallet) { return wallet->getTotalBalance(); });
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

void WalletsManager::onNewBlock()
{
   logger_->debug("[WalletsManager::{}] new Block", __func__);
   updateWallets();
   emit blockchainEvent();
}

void WalletsManager::onRefresh(std::vector<BinaryData> ids, bool online)
{
   if (!online) {
      return;
   }
   if (settlementWallet_) {   //TODO: check for refresh id
      settlementWallet_->refreshWallets(ids);
   }

   emit blockchainEvent();
}

void WalletsManager::onStateChanged(ArmoryConnection::State state)
{
   if (state == ArmoryConnection::State::Ready) {
      logger_->debug("[WalletsManager::{}] - DB ready", __func__);
      resumeRescan();
      updateWallets();
      emit walletsReady();
   }
   else {
      logger_->debug("[WalletsManager::{}] -  Armory state changed: {}"
         , __func__, (int)state);
   }
}

void WalletsManager::onWalletReady(const QString &walletId)
{
   emit walletReady(walletId);
   if (!armory_) {
      return;
   }
   if (armory_->state() != ArmoryConnection::State::Ready) {
      readyWallets_.insert(walletId);
      auto nbWallets = wallets_.size();
      if (settlementWallet_ != nullptr) {
         nbWallets++;
      }
      if (readyWallets_.size() >= nbWallets) {
         logger_->debug("[WalletsManager::{}] - All wallets are ready", __func__);
         armory_->goOnline();
         readyWallets_.clear();
      }
   }
}

void WalletsManager::onWalletImported(const std::string &walletId)
{
   logger_->debug("[WalletsManager::{}] - HD wallet {} imported", __func__
      , walletId);
   updateWallets(true);
   emit walletImportFinished(walletId);
}

bool WalletsManager::isArmoryReady() const
{
   return (armory_ && (armory_->state() == ArmoryConnection::State::Ready));
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

bool WalletsManager::deleteWallet(const WalletPtr &wallet)
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
      if (wallet == settlementWallet_) {
         settlementWallet_ = nullptr;
      }
      eraseWallet(wallet);
   }

   if (authAddressWallet_ == wallet) {
      authAddressWallet_ = nullptr;
      emit authWalletChanged();
   }
   emit walletBalanceUpdated(wallet->walletId());
   return true;
}

bool WalletsManager::deleteWallet(const HDWalletPtr &wallet)
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

void WalletsManager::registerWallets()
{
   if (!armory_) {
      return;
   }
   if (empty()) {
      logger_->debug("[WalletsManager::{}] - No wallets to register.", __func__);
      return;
   }
   for (auto &it : wallets_) {
      it.second->registerWallet(armory_);
   }
   if (settlementWallet_) {
       settlementWallet_->registerWallet(armory_);
   }
}

void WalletsManager::unregisterWallets()
{
   for (auto &it : wallets_) {
      it.second->unregisterWallet();
   }
   if (settlementWallet_) {
      settlementWallet_->unregisterWallet();
   }
}

void WalletsManager::updateWallets(bool force)
{
   for (auto &it : wallets_) {
      it.second->firstInit(force);
   }
   if (settlementWallet_) {
      settlementWallet_->firstInit(force);
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
            const bs::Address addr(prevOut.getScrAddressStr());
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
               bs::PayoutSigner::WhichSignature(tx, 0, settlAE, logger_, armory_, cbPayout);
               return;
            }
            logger_->warn("[WalletsManager::{}] - failed to get settlement AE"
               , __func__);
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
      bool isOurs = (getWalletByAddress(addr.id()) == wallet);
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

void WalletsManager::createWallet(const std::string& name, const std::string& description
   , bs::core::wallet::Seed seed, bool primary
   , const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank)
{
   if (!signContainer_) {
      logger_->error("[WalletsManager::{}] - signer is not set - aborting"
         , __func__);
      return;
   }
   createHdReqId_ = signContainer_->createHDWallet(name, description, primary
      , seed, pwdData, keyRank);
}

void WalletsManager::onHDWalletCreated(unsigned int id, std::shared_ptr<bs::sync::hd::Wallet> newWallet)
{
   if (id != createHdReqId_) {
      return;
   }
   createHdReqId_ = 0;
   newWallet->synchronize([] {});
   adoptNewWallet(newWallet);
   emit walletCreated(newWallet);
}

void WalletsManager::startWalletRescan(const HDWalletPtr &hdWallet)
{
   const auto &cbr = [this](const std::string &walletId) -> unsigned int {
      return 0;
   };
   const auto &cbw = [this](const std::string &walletId, unsigned int idx) {
      appSettings_->SetWalletScanIndex(walletId, idx);
   };

   if (armory_->state() == ArmoryConnection::State::Ready) {
      hdWallet->startRescan([this](bs::sync::hd::Group *grp, bs::hd::Path::Elem wallet, bool isValid) {
         logger_->debug("[WalletsManager::startWalletRescan] finished scan of {}: {}", wallet, isValid);
      }, cbr, cbw);
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
            QMetaObject::invokeMethod(this, [this, hdWallet] { emit walletCreated(hdWallet); });
            logger_->debug("[WalletsManager::onWalletsListUpdated] found new wallet {} "
               "- starting address scan for it", hdWalletId);
            startWalletRescan(hdWallet);
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

void WalletsManager::adoptNewWallet(const HDWalletPtr &wallet)
{
   saveWallet(wallet);
   if (armory_) {
      wallet->registerWallet(armory_);
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
      wallet->registerWallet(armory_);
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
   for (const auto &wallet : wallets_) {
      if (wallet.second->type() != bs::core::wallet::Type::ColorCoin) {
         continue;
      }
      if (wallet.second->shortName() == cc) {
         wallet.second->setDescription(ccDesc.toStdString());
         const auto ccWallet = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(wallet.second);
         if (ccWallet) {
            ccWallet->setData(genesisAddr.toStdString());
            ccWallet->setData(nbSatoshis);
         }
         else {
            logger_->warn("[WalletsManager::{}] - Invalid CC leaf type for {}"
               , __func__, ccProd.toStdString());
         }
      }
   }
   ccSecurities_[cc] = { ccDesc.toStdString(), nbSatoshis, genesisAddr.toStdString() };
}

void WalletsManager::onCCInfoLoaded()
{
   logger_->debug("[WalletsManager::{}] - Re-validating against GAs in CC leaves"
      , __func__);
   for (const auto &hdWallet : hdWallets_) {
      for (const auto &leaf : hdWallet.second->getLeaves()) {
         if (leaf->type() == bs::core::wallet::Type::ColorCoin) {
            leaf->firstInit();
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
void WalletsManager::onZeroConfReceived(const std::vector<bs::TXEntry> entries)
{
   std::vector<bs::TXEntry> ourZCentries;

   for (const auto &entry : entries) {
      auto wallet = getWalletById(entry.id);
      if (wallet != nullptr) {
         logger_->debug("[WalletsManager::{}] - ZC entry in wallet {}", __func__
            , wallet->name());

         // We have an affected wallet. Update it!
         ourZCentries.push_back(entry);
         wallet->updateBalances();
      } // if
      else {
         logger_->debug("[WalletsManager::{}] - get ZC but wallet not found: {}"
            , __func__, entry.id);
      }
   } // for

     // Emit signals for the wallet and TX view models.
   emit blockchainEvent();
   if (!ourZCentries.empty()) {
      emit newTransactions(ourZCentries);
   }
}

void WalletsManager::onZeroConfInvalidated(const std::vector<bs::TXEntry> entries)
{
   if (!entries.empty()) {
      emit invalidatedZCs(entries);
   }
}

void WalletsManager::onBroadcastZCError(const QString &txHash, const QString &errMsg)
{
   logger_->error("[WalletsManager::{}] - TX {} error: {}", __func__
      , txHash.toStdString(), errMsg.toStdString());
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
   armory_->estimateFee(blocks, cbFee);
   return true;
}

void WalletsManager::resumeRescan()
{
   if (!appSettings_) {
      return;
   }
   std::unordered_map<std::string, std::shared_ptr<bs::sync::hd::Wallet>> rootWallets;
   for (const auto &resumeIdx : appSettings_->UnfinishedWalletsRescan()) {
      const auto &rootWallet = getHDRootForLeaf(resumeIdx.first);
      if (!rootWallet) {
         continue;
      }
      rootWallets[rootWallet->walletId()] = rootWallet;
   }
   if (rootWallets.empty()) {
      return;
   }

   const auto &cbr = [this] (const std::string &walletId) -> unsigned int {
      return appSettings_->GetWalletScanIndex(walletId);
   };
   const auto &cbw = [this] (const std::string &walletId, unsigned int idx) {
      appSettings_->SetWalletScanIndex(walletId, idx);
   };
   logger_->debug("[WalletsManager::{}] - Resuming blockchain rescan for {} "
      "root wallet[s]", __func__, rootWallets.size());
   for (const auto &rootWallet : rootWallets) {
      if (rootWallet.second->startRescan(nullptr, cbr, cbw)) {
         emit walletImportStarted(rootWallet.first);
      }
      else {
         logger_->warn("[WalletsManager::{}] - Rescan for {} is already in "
            "progress", __func__, rootWallet.second->name());
      }
   }
}
