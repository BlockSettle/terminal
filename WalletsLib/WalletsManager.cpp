#include "WalletsManager.h"

#include "ApplicationSettings.h"
#include "FastLock.h"
#include "HDWallet.h"

#include <QDir>
#include <QMutexLocker>

#include <spdlog/spdlog.h>
#include <btc/ecc.h>


WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<ApplicationSettings>& appSettings
 , const std::shared_ptr<ArmoryConnection> &armory, bool preferWatchinOnly)
   : appSettings_(appSettings)
   , logger_(logger)
   , armory_(armory)
   , preferWatchingOnly_(preferWatchinOnly)
{
   btc_ecc_start();

   nbBackupFilesToKeep_ = appSettings_->get<unsigned int>(ApplicationSettings::nbBackupFilesKeep);
   if (armory_) {
      connect(armory_.get(), &ArmoryConnection::zeroConfReceived, this, &WalletsManager::onZeroConfReceived, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::txBroadcastError, this, &WalletsManager::onBroadcastZCError, Qt::QueuedConnection);
      connect(armory_.get(), SIGNAL(stateChanged(ArmoryConnection::State)), this, SLOT(onStateChanged(ArmoryConnection::State)), Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::newBlock, this, &WalletsManager::onRefresh, Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::refresh, this, &WalletsManager::onRefresh, Qt::QueuedConnection);
   }
}

WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger), preferWatchingOnly_(false)
{
   btc_ecc_start();
}

WalletsManager::~WalletsManager()
{
   btc_ecc_stop();
}

void WalletsManager::Reset()
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

void WalletsManager::LoadWallets(NetworkType netType, const QString &walletsPath, const load_progress_delegate &progressDelegate)
{
   QDir walletsDir(walletsPath);

   if (!walletsDir.exists()) {
      logger_->debug("Creating wallets path {}", walletsPath.toStdString());
      walletsDir.mkpath(walletsPath);
   }

   QStringList filesFilter{QString::fromStdString("*.lmdb")};
   auto fileList = walletsDir.entryList(filesFilter, QDir::Files);

   int totalCount = 1;
   int current = 0;
   int ratio = 0;

   for (const auto& file : fileList) {
      if (IsWalletFile(file)) {
         ++totalCount;
      }
   }
   if (totalCount != 0) {
      ratio = 100 / totalCount;
   }

   std::vector<std::shared_ptr<bs::hd::Wallet>> hdSigningWallets, hdWoWallets;
   const auto errorTitle = tr("Load wallet error");

   for (const auto& file : fileList) {
      if (!IsWalletFile(file)) {
         continue;
      }
      QFileInfo fileInfo(walletsDir.absoluteFilePath(file));
      try {
         logger_->debug("Loading BIP44 wallet from {}", file.toStdString());
         const auto &wallet = std::make_shared<bs::hd::Wallet>(fileInfo.absoluteFilePath().toStdString());
         current += ratio;
         if (progressDelegate) {
            progressDelegate(current);
         }
         if (netType != wallet->networkType()) {
            if (progressDelegate) { // loading in the terminal
               emit error(errorTitle, tr("Wallet %1 (from file %2) has incompatible network type")
                  .arg(QString::fromStdString(wallet->getName())).arg(file));
               continue;
            }
            else {
               logger_->warn("Network type mismatch: loading {}, wallet has {}", (int)netType, (int)wallet->networkType());
            }
         }

         if (file.startsWith(QString::fromStdString(bs::hd::Wallet::fileNamePrefix(false)))) {
            hdSigningWallets.emplace_back(wallet);
         }
         else if (progressDelegate && file.startsWith(QString::fromStdString(bs::hd::Wallet::fileNamePrefix(true)))) {
            hdWoWallets.emplace_back(wallet);
         }
      }
      catch (const std::exception &e) {
         logger_->warn("Failed to load BIP44 wallet: {}", e.what());
         emit error(errorTitle, QLatin1String(e.what()));
      }
   }

   if (!hdWoWallets.empty()) {
      for (const auto &hdWallet : hdWoWallets) {
         if (!hdWallet->isWatchingOnly()) {
            logger_->error("Signing wallet pretends as a watching-only one - skipping");
            emit error(errorTitle, tr("Wallet %1 (id %2): invalid watching-only format - skipped")
               .arg(QString::fromStdString(hdWallet->getName())).arg(QString::fromStdString(hdWallet->getWalletId())));
            continue;
         }
         if (hdWallet->isPrimary() && HasPrimaryWallet()) {
            logger_->error("Wallet {} ({}) is skipped - loading of multiple primary wallets is not supported!"
               , hdWallet->getName(), hdWallet->getWalletId());
            emit error(errorTitle, tr("Wallet %1 (id %2) is skipped - multiple primary wallets are not supported")
               .arg(QString::fromStdString(hdWallet->getName())).arg(QString::fromStdString(hdWallet->getWalletId())));
            continue;
         }
         SaveWallet(hdWallet);
      }
   }
   else {
      if (preferWatchingOnly_ && !hdSigningWallets.empty()) {
         logger_->warn("Failed to find watching-only wallets - temporary fallback to signing ones");
         emit info(tr("Wallets recommendation")
            , tr("The Terminal has detected a signing wallet, not a watching-only wallet. Please consider replacing"
               " the signing wallet with a watching-only wallet."));
      }
      for (const auto &hdWallet : hdSigningWallets) {
         if (hdWallet->isPrimary() && HasPrimaryWallet()) {
            logger_->error("Wallet {} ({}) is not loaded - multiple primary wallets are not supported!"
               , hdWallet->getName(), hdWallet->getWalletId());
            continue;
         }
         SaveWallet(hdWallet);
      }
   }

   try {
      if (bs::SettlementWallet::exists(walletsPath.toStdString(), netType)) {
         logger_->debug("Loading settlement wallet");
         settlementWallet_ = bs::SettlementWallet::loadFromFolder(walletsPath.toStdString(), netType);
         connect(settlementWallet_.get(), &bs::SettlementWallet::walletReady, this, &WalletsManager::onWalletReady);
      }
   }
   catch (const WalletException &e) {
      logger_->error("Failed to load settlement wallet: {}", e.what());
      emit error(errorTitle, tr("Failed to load settlement wallet: %1").arg(QLatin1String(e.what())));
   }
   emit walletsLoaded();
}

void WalletsManager::BackupWallet(const hd_wallet_type &wallet, const std::string &targetDir) const
{
   if (wallet->isWatchingOnly()) {
      logger_->info("No need to backup watching-only wallet {}", wallet->getName());
      return;
   }
   const QString backupDir = QString::fromStdString(targetDir);
   QDir dirBackup(backupDir);
   if (!dirBackup.exists()) {
      if (!dirBackup.mkpath(backupDir)) {
         logger_->error("Failed to create backup directory {}", targetDir);
         emit error(tr("Wallet Backup error"), tr("Failed to create backup directory %1").arg(backupDir));
         return;
      }
   }
   const auto &lockFiles = dirBackup.entryList(QStringList{ QLatin1String("*.lmdb-lock") });
   for (const auto &file : lockFiles) {
      QFile::remove(backupDir + QDir::separator() + file);
   }
   auto files = dirBackup.entryList(QStringList{ QString::fromStdString(
      bs::hd::Wallet::fileNamePrefix(false) + wallet->getWalletId() + "_*.lmdb" )});
   if (!files.empty() && (files.size() >= nbBackupFilesToKeep_)) {
      for (int i = 0; i <= files.size() - nbBackupFilesToKeep_; i++) {
         logger_->debug("Removing old backup file {}", files[i].toStdString());
         QFile::remove(backupDir + QDir::separator() + files[i]);
      }
   }
   const auto curTime = QDateTime::currentDateTime().toLocalTime().toString(QLatin1String("yyyyMMddHHmmss"));
   const auto backupFile = targetDir + "/" + bs::hd::Wallet::fileNamePrefix(false)
      + wallet->getWalletId()  + "_" + curTime.toStdString() + ".lmdb";
   wallet->copyToFile(backupFile);
}

bool WalletsManager::IsWalletFile(const QString& fileName) const
{
   if (fileName.startsWith(QString::fromStdString(bs::SettlementWallet::fileNamePrefix()))) {
      return false;
   }
   return true;
}

bool WalletsManager::IsReadyForTrading() const
{
   return (HasPrimaryWallet() && HasSettlementWallet());
}

bool WalletsManager::CreateSettlementWallet(NetworkType netType, const QString &walletsPath)
{
   logger_->debug("Creating settlement wallet");
   try {
      settlementWallet_ = bs::SettlementWallet::create(walletsPath.toStdString(), netType);
   }
   catch (const std::exception &e) {
      logger_->error("Failed to create Settlement wallet: {}", e.what());
   }
   if (settlementWallet_ != nullptr) {
      connect(settlementWallet_.get(), &bs::SettlementWallet::walletReady, this, &WalletsManager::onWalletReady);
      if (armory_) {
         settlementWallet_->RegisterWallet(armory_);
      }
      emit walletChanged();
   }
   return (settlementWallet_ != nullptr);
}

void WalletsManager::SaveWallet(const wallet_gen_type& newWallet)
{
   if (hdDummyWallet_ == nullptr) {
      hdDummyWallet_ = std::make_shared<bs::hd::DummyWallet>();
      hdWalletsId_.emplace_back(hdDummyWallet_->getWalletId());
      hdWallets_[hdDummyWallet_->getWalletId()] = hdDummyWallet_;
   }
   AddWallet(newWallet);
}

void WalletsManager::AddWallet(const wallet_gen_type &wallet, bool isHDLeaf)
{
   if (!isHDLeaf) {
      hdDummyWallet_->add(wallet);
   }
   {
      QMutexLocker lock(&mtxWallets_);
      walletsId_.emplace_back(wallet->GetWalletId());
      wallets_.emplace(wallet->GetWalletId(), wallet);
   }
   connect(wallet.get(), &bs::Wallet::walletReady, this, &WalletsManager::onWalletReady);
   connect(wallet.get(), &bs::Wallet::addressAdded, [this] { emit walletChanged(); });
   connect(wallet.get(), &bs::Wallet::walletReset, [this] { emit walletChanged(); });
}

void WalletsManager::SaveWallet(const hd_wallet_type &wallet)
{
   if (!userId_.isNull()) {
      wallet->setUserId(userId_);
   }
   connect(wallet.get(), &bs::hd::Wallet::leafAdded, this, &WalletsManager::onHDLeafAdded);
   connect(wallet.get(), &bs::hd::Wallet::leafDeleted, this, &WalletsManager::onHDLeafDeleted);
   connect(wallet.get(), &bs::hd::Wallet::scanComplete, this, &WalletsManager::onWalletImported, Qt::QueuedConnection);
   hdWalletsId_.emplace_back(wallet->getWalletId());
   hdWallets_[wallet->getWalletId()] = wallet;
   walletNames_.insert(wallet->getName());
   for (const auto &leaf : wallet->getLeaves()) {
      AddWallet(leaf, true);
   }
}

void WalletsManager::SetAuthWalletFrom(const hd_wallet_type &wallet)
{
   const auto group = wallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   if ((group != nullptr) && (group->getNumLeaves() > 0)) {
      authAddressWallet_ = group->getLeaf(0);
   }
}

void WalletsManager::onHDLeafAdded(QString id)
{
   logger_->debug("HD leaf {} added", id.toStdString());
   for (const auto &hdWallet : hdWallets_) {
      const auto &leaf = hdWallet.second->getLeaf(id.toStdString());
      if (leaf == nullptr) {
         continue;
      }

      const auto &ccIt = ccSecurities_.find(leaf->GetShortName());
      if (ccIt != ccSecurities_.end()) {
         leaf->SetDescription(ccIt->second);
      }

      leaf->SetUserID(userId_);
      AddWallet(leaf, true);
      if (armory_) {
         leaf->RegisterWallet(armory_);
      }

      if (authAddressWallet_ == nullptr) {
         SetAuthWalletFrom(hdWallet.second);
         if (authAddressWallet_ != nullptr) {
            emit authWalletChanged();
         }
      }
      emit walletChanged();
      break;
   }
}

void WalletsManager::onHDLeafDeleted(QString id)
{
   const auto &wallet = GetWalletById(id.toStdString());
   EraseWallet(wallet);
   emit walletChanged();
}

WalletsManager::hd_wallet_type WalletsManager::GetPrimaryWallet() const
{
   for (const auto &wallet : hdWallets_) {
      if (wallet.second->isPrimary()) {
         return wallet.second;
      }
   }
   return nullptr;
}

bool WalletsManager::HasPrimaryWallet() const
{
   return (GetPrimaryWallet() != nullptr);
}

WalletsManager::wallet_gen_type WalletsManager::GetDefaultWallet() const
{
   wallet_gen_type result;
   const auto &priWallet = GetPrimaryWallet();
   if (priWallet) {
      const auto &group = priWallet->getGroup(priWallet->getXBTGroupType());
      result = group ? group->getLeaf(0) : nullptr;
   }
   return result;
}

WalletsManager::wallet_gen_type WalletsManager::GetCCWallet(const std::string &cc)
{
   if (cc.empty() || !HasPrimaryWallet()) {
      return nullptr;
   }
   if ((cc.length() == 1) && (cc[0] >= '0') && (cc[0] <= '9')) {
      return nullptr;
   }
   const auto &priWallet = GetPrimaryWallet();
   auto ccGroup = priWallet->getGroup(bs::hd::CoinType::BlockSettle_CC);
   if (ccGroup == nullptr) {
      ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   }
   return ccGroup->getLeaf(cc);
}

void WalletsManager::SetUserId(const BinaryData &userId)
{
   userId_ = userId;
   for (const auto &hdWallet : hdWallets_) {
      hdWallet.second->setUserId(userId);
   }
}

const WalletsManager::hd_wallet_type WalletsManager::GetHDWallet(const unsigned int index) const
{
   if (index >= hdWalletsId_.size()) {
      return nullptr;
   }
   return GetHDWalletById(hdWalletsId_[index]);
}

const WalletsManager::hd_wallet_type WalletsManager::GetHDWalletById(const std::string& walletId) const
{
   auto it = hdWallets_.find(walletId);
   if (it != hdWallets_.end()) {
      return it->second;
   }
   return nullptr;
}

const WalletsManager::hd_wallet_type WalletsManager::GetHDRootForLeaf(const std::string& walletId) const
{
   for (const auto &hdWallet : hdWallets_) {
      if (hdWallet.second->getLeaf(walletId)) {
         return hdWallet.second;
      }
   }
   return nullptr;
}

WalletsManager::wallet_gen_type WalletsManager::GetWallet(const unsigned int index) const
{
   if (index > wallets_.size()) {
      return nullptr;
   } else if (index == wallets_.size()) {
      return settlementWallet_;
   }

   return GetWalletById(walletsId_[index].toBinStr());
}

bool WalletsManager::WalletNameExists(const std::string &walletName) const
{
   const auto &it = walletNames_.find(walletName);
   return (it != walletNames_.end());
}

BTCNumericTypes::balance_type WalletsManager::GetSpendableBalance() const
{
   if (!IsArmoryReady()) {
      return -1;
   }
   // TODO: make it lazy init
   BTCNumericTypes::balance_type totalSpendable = 0;

   for (const auto& it : wallets_) {
      if (it.second->GetType() != bs::wallet::Type::Bitcoin) {
         continue;
      }
      const auto walletSpendable = it.second->GetSpendableBalance();
      if (walletSpendable > 0) {
         totalSpendable += walletSpendable;
      }
   }
   return totalSpendable;
}

BTCNumericTypes::balance_type WalletsManager::GetUnconfirmedBalance() const
{
   if (!IsArmoryReady()) {
      return 0;
   }
   // TODO: make it lazy init
   BTCNumericTypes::balance_type totalUnconfirmed = 0;

   for (const auto& it : wallets_) {
      totalUnconfirmed += it.second->GetUnconfirmedBalance();
   }

   return totalUnconfirmed;
}

BTCNumericTypes::balance_type WalletsManager::GetTotalBalance() const
{
   if (!IsArmoryReady()) {
      return 0;
   }
   BTCNumericTypes::balance_type totalBalance = 0;

   for (const auto& it : wallets_) {
      totalBalance += it.second->GetTotalBalance();
   }

   return totalBalance;
}

void WalletsManager::onRefresh()
{
   logger_->debug("[WalletsManager] Armory refresh");
   UpdateSavedWallets();
   emit blockchainEvent();
}

void WalletsManager::onStateChanged(ArmoryConnection::State state)
{
   if (state == ArmoryConnection::State::Ready) {
      logger_->debug("[WalletsManager] DB ready");
      ResumeRescan();
      UpdateSavedWallets();
      emit walletsReady();
   }
   else {
      logger_->debug("[WalletsManager] Armory state changed: {}", (int)state);
   }
}

void WalletsManager::onWalletReady(const QString &walletId)
{
   emit walletReady(walletId);
   if (!armory_) {
      return;
   }
   readyWallets_.insert(walletId);
   auto nbWallets = wallets_.size();
   if (settlementWallet_ != nullptr) {
      nbWallets++;
   }
   if (readyWallets_.size() >= nbWallets) {     
      logger_->debug("All wallets are ready - going online");
      armory_->goOnline();
      readyWallets_.clear();
   }
}

bool WalletsManager::IsArmoryReady() const
{
   return (armory_ && (armory_->state() == ArmoryConnection::State::Ready));
}

WalletsManager::wallet_gen_type WalletsManager::GetWalletById(const std::string& walletId) const
{
   {
      auto it = wallets_.find(walletId);
      if (it != wallets_.end()) {
         return it->second;
      }
   }
   if (settlementWallet_ && settlementWallet_->hasWalletId(walletId)) {
      return settlementWallet_;
   }
   return nullptr;
}

WalletsManager::wallet_gen_type WalletsManager::GetWalletByAddress(const bs::Address &addr) const
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

void WalletsManager::EraseWallet(const wallet_gen_type &wallet)
{
   if (!wallet) {
      return;
   }
   QMutexLocker lock(&mtxWallets_);
   const auto itId = std::find(walletsId_.begin(), walletsId_.end(), wallet->GetWalletId());
   if (itId != walletsId_.end()) {
      walletsId_.erase(itId);
   }
   wallets_.erase(wallet->GetWalletId());
}

bool WalletsManager::DeleteWalletFile(const wallet_gen_type &wallet)
{
   bool isHDLeaf = false;
   logger_->info("Removing wallet {} ({})...", wallet->GetWalletName(), wallet->GetWalletId());
   for (auto hdWallet : hdWallets_) {
      const auto leaves = hdWallet.second->getLeaves();
      if (std::find(leaves.begin(), leaves.end(), wallet) != leaves.end()) {
         for (auto group : hdWallet.second->getGroups()) {
            if (group->deleteLeaf(wallet)) {
               isHDLeaf = true;
               EraseWallet(wallet);
               break;
            }
         }
      }
      if (isHDLeaf) {
         break;
      }
   }
   if (!isHDLeaf) {
      if (!wallet->EraseFile()) {
         logger_->error("Failed to remove wallet file for {}", wallet->GetWalletName());
         return false;
      }
      if (wallet == settlementWallet_) {
         settlementWallet_ = nullptr;
      }
      EraseWallet(wallet);
   }

   if (authAddressWallet_ == wallet) {
      authAddressWallet_ = nullptr;
      emit authWalletChanged();
   }
   emit walletChanged();
   return true;
}

bool WalletsManager::DeleteWalletFile(const hd_wallet_type &wallet)
{
   const auto it = hdWallets_.find(wallet->getWalletId());
   if (it == hdWallets_.end()) {
      logger_->warn("Unknown HD wallet {} ({})", wallet->getName(), wallet->getWalletId());
      return false;
   }

   const auto &leaves = wallet->getLeaves();
   for (const auto &leaf : leaves) {
      EraseWallet(leaf);
   }

   const auto itId = std::find(hdWalletsId_.begin(), hdWalletsId_.end(), wallet->getWalletId());
   if (itId != hdWalletsId_.end()) {
      hdWalletsId_.erase(itId);
   }
   hdWallets_.erase(wallet->getWalletId());
   walletNames_.erase(wallet->getName());
   const bool result = wallet->eraseFile();
   logger_->info("Wallet {} ({}) removed: {}", wallet->getName(), wallet->getWalletId(), result);

   if (!GetPrimaryWallet()) {
      authAddressWallet_.reset();
      emit authWalletChanged();
   }
   emit walletChanged();
   return result;
}

uint32_t WalletsManager::GetTopBlockHeight() const
{
   if (!armory_) {
      return UINT32_MAX;
   }
   return armory_->topBlock();
}

void WalletsManager::RegisterSavedWallets()
{
   if (!armory_) {
      return;
   }
   if (empty()) {
      logger_->debug("Going online before wallets are added");
      armory_->goOnline();
      return;
   }
   {
      for (auto &it : wallets_) {
         it.second->RegisterWallet(armory_);
      }
   }
   if (settlementWallet_) {
       settlementWallet_->RegisterWallet(armory_);
   }
}

void WalletsManager::UpdateSavedWallets()
{
   for (auto &it : wallets_) {
      it.second->firstInit();
   }
   if (settlementWallet_) {
      settlementWallet_->firstInit();
   }
}

bool WalletsManager::IsTransactionVerified(const ClientClasses::LedgerEntry &item)
{
   return (armory_ && armory_->isTransactionVerified(item));
}

bool WalletsManager::GetTransactionDirection(Tx tx, const std::shared_ptr<bs::Wallet> &wallet
   , std::function<void(bs::Transaction::Direction)> cb)
{
   if (!tx.isInitialized() || !armory_ || !wallet) {
      return false;
   }

   BinaryData txHash = tx.getThisHash();
   if (wallet->GetType() == bs::wallet::Type::Authentication) {
      cb(bs::Transaction::Auth);
      return true;
   }
   else if (wallet->GetType() == bs::wallet::Type::ColorCoin) {
      cb(bs::Transaction::Delivery);
      return true;
   }

   bs::Transaction::Direction dir = bs::Transaction::Direction::Unknown;
   {
      FastLock lock(txDirLock_);
      const auto &itDirCache = txDirections_.find(txHash);
      if (itDirCache != txDirections_.end()) {
         dir = itDirCache->second;
      }
   }
   if (dir != bs::Transaction::Direction::Unknown) {
      cb(dir);
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

   const auto &cbProcess = [this, wallet, tx, txHash, txOutIndices, cb](std::vector<Tx> txs) {
      bool ourOuts = false;
      bool otherOuts = false;
      bool ourIns = false;
      bool otherIns = false;
      bool ccTx = false;

      std::vector<TxOut> txOuts;
      txOuts.reserve(tx.getNumTxIn());

      for (const auto &prevTx : txs) {
         const auto &itIdx = txOutIndices.find(prevTx.getThisHash());
         if (itIdx == txOutIndices.end()) {
            continue;
         }
         for (const auto idx : itIdx->second) {
            TxOut prevOut = prevTx.getTxOutCopy((int)idx);
            const auto &addrWallet = GetWalletByAddress(prevOut.getScrAddressStr());
            ((addrWallet == wallet) ? ourIns : otherIns) = true;
            if (addrWallet && (addrWallet->GetType() == bs::wallet::Type::ColorCoin)) {
               ccTx = true;
            }
            txOuts.emplace_back(prevOut);
         }
      }

      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy((int)i);
         const auto addrWallet = GetWalletByAddress(out.getScrAddressStr());
         ((addrWallet == wallet) ? ourOuts : otherOuts) = true;
         if (addrWallet && (addrWallet->GetType() == bs::wallet::Type::ColorCoin)) {
            ccTx = true;
         }
      }

      if (wallet->GetType() == bs::wallet::Type::Settlement) {
         if (ourOuts) {
            updateTxDirCache(txHash, bs::Transaction::PayIn, cb);
            return;
         }
         if (txOuts.size() == 1) {
            const auto addr = txOuts[0].getScrAddressStr();
            const auto settlAE = dynamic_pointer_cast<bs::SettlementAddressEntry>(GetSettlementWallet()->getAddressEntryForAddr(addr));
            if (settlAE) {
               const auto &cbPayout = [this, cb, txHash](bs::PayoutSigner::Type poType) {
                  if (poType == bs::PayoutSigner::SignedBySeller) {
                     updateTxDirCache(txHash, bs::Transaction::Revoke, cb);
                  }
                  else {
                     updateTxDirCache(txHash, bs::Transaction::PayOut, cb);
                  }
               };
               bs::PayoutSigner::WhichSignature(tx, 0, settlAE, logger_, armory_, cbPayout);
               return;
            }
         }
         updateTxDirCache(txHash, bs::Transaction::PayOut, cb);
         return;
      }

      if (ccTx) {
         updateTxDirCache(txHash, bs::Transaction::Payment, cb);
         return;
      }
      if (ourOuts && ourIns && !otherOuts && !otherIns) {
         updateTxDirCache(txHash, bs::Transaction::Internal, cb);
         return;
      }
      if (!ourIns) {
         updateTxDirCache(txHash, bs::Transaction::Received, cb);
         return;
      }
      if (otherOuts) {
         updateTxDirCache(txHash, bs::Transaction::Sent, cb);
         return;
      }
      updateTxDirCache(txHash, bs::Transaction::Unknown, cb);
   };
   armory_->getTXsByHash(opTxHashes, cbProcess);
   return true;
}

bool WalletsManager::GetTransactionMainAddress(const Tx &tx, const std::shared_ptr<bs::Wallet> &wallet
   , bool isReceiving, std::function<void(QString)> cb)
{
   if (!tx.isInitialized() || !armory_ || !wallet) {
      return false;
   }

   const bool isSettlement = (wallet->GetType() == bs::wallet::Type::Settlement);
   std::set<bs::Address> addresses;
   const auto txHash = tx.getThisHash();

   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy((int)i);
      const auto addr = bs::Address::fromTxOut(out);
      bool isOurs = (GetWalletByAddress(addr.id()) == wallet);
      if ((isOurs == isReceiving) || (isOurs && isSettlement)) {
         addresses.insert(addr);
      }
   }

   const auto &cbProcessAddresses = [this, txHash, cb](const std::set<bs::Address> &addresses) {
      switch (addresses.size()) {
      case 0:
         updateTxDescCache(txHash, tr("no address"), cb);
         break;

      case 1:
         updateTxDescCache(txHash, (*addresses.begin()).display(), cb);
         break;

      default:
         updateTxDescCache(txHash, tr("%1 output addresses").arg(addresses.size()), cb);
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

      const auto &cbProcess = [this, txHash, txOutIndices, wallet, cbProcessAddresses](std::vector<Tx> txs) {
         std::set<bs::Address> addresses;
         for (const auto &prevTx : txs) {
            const auto &itIdx = txOutIndices.find(prevTx.getThisHash());
            if (itIdx == txOutIndices.end()) {
               continue;
            }
            for (const auto idx : itIdx->second) {
               const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy((int)idx));
               if (GetWalletByAddress(addr) == wallet) {
                  addresses.insert(addr);
               }
            }
            cbProcessAddresses(addresses);
         }
      };
      armory_->getTXsByHash(opTxHashes, cbProcess);
   }
   else {
      cbProcessAddresses(addresses);
   }
   return true;
}

void WalletsManager::updateTxDirCache(const BinaryData &txHash, bs::Transaction::Direction dir
   , std::function<void(bs::Transaction::Direction)> cb)
{
   {
      FastLock lock(txDirLock_);
      txDirections_[txHash] = dir;
   }
   cb(dir);
}

void WalletsManager::updateTxDescCache(const BinaryData &txHash, const QString &desc, std::function<void(QString)> cb)
{
   {
      FastLock lock(txDescLock_);
      txDesc_[txHash] = desc;
   }
   cb(desc);
}

WalletsManager::hd_wallet_type WalletsManager::CreateWallet(const std::string& name, const std::string& description
   , bs::wallet::Seed seed, const QString &walletsPath, bool primary
   , const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank)
{
   if (preferWatchingOnly_) {
      throw std::runtime_error("Can't create wallets in watching-only mode");
   }

   const auto newWallet = std::make_shared<bs::hd::Wallet>(name, description, seed);

   if (hdWallets_.find(newWallet->getWalletId()) != hdWallets_.end()) {
      throw std::runtime_error("HD wallet with id " + newWallet->getWalletId() + " already exists");
   }

   newWallet->createStructure();
   if (primary) {
      newWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
   }
   if (!pwdData.empty()) {
      newWallet->changePassword(pwdData, keyRank);
   }
   AdoptNewWallet(newWallet, walletsPath);
   return newWallet;
}

void WalletsManager::AdoptNewWallet(const hd_wallet_type &wallet, const QString &walletsPath)
{
   wallet->saveToDir(walletsPath.toStdString());
   SaveWallet(wallet);
   if (armory_) {
      wallet->RegisterWallet(armory_);
   }
   emit newWalletAdded(wallet->getWalletId());
   emit walletsReady();
}

void WalletsManager::AddWallet(const hd_wallet_type &wallet, const QString &walletsPath)
{
   if (!wallet) {
      return;
   }
   wallet->saveToDir(walletsPath.toStdString());
   SaveWallet(wallet);
   if (armory_) {
      wallet->RegisterWallet(armory_);
      emit walletsReady();
   }
}

void WalletsManager::onCCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr)
{
   const auto &cc = ccProd.toStdString();
   for (const auto &wallet : wallets_) {
      if (wallet.second->GetType() != bs::wallet::Type::ColorCoin) {
         continue;
      }
      if (wallet.second->GetShortName() == cc) {
         wallet.second->SetDescription(ccDesc.toStdString());
         const auto ccWallet = dynamic_pointer_cast<bs::hd::Leaf>(wallet.second);
         if (ccWallet) {
            ccWallet->setData(genesisAddr.toStdString());
            ccWallet->setData(nbSatoshis);
         }
         else {
            logger_->warn("Invalid CC leaf type for {}", ccProd.toStdString());
         }
      }
   }
   ccSecurities_[cc] = ccDesc.toStdString();
}

void WalletsManager::onCCInfoLoaded()
{
   logger_->debug("Re-validating against GAs in CC leaves");
   for (const auto &hdWallet : hdWallets_) {
      for (const auto &leaf : hdWallet.second->getLeaves()) {
         if (leaf->GetType() == bs::wallet::Type::ColorCoin) {
            leaf->firstInit();
         }
      }
   }
}

void WalletsManager::onZeroConfReceived(ArmoryConnection::ReqIdType reqId)
{
   unsigned int foundCnt = 0;
   for (const auto led : armory_->getZCentries(reqId)) {
      auto wallet = GetWalletById(led.getID());
      if (wallet != nullptr) {
         foundCnt++;
         wallet->AddUnconfirmedBalance(led.getValue() / BTCNumericTypes::BalanceDivider);
      }
   }
   if (foundCnt) {
      emit blockchainEvent();
   }
}

void WalletsManager::onBroadcastZCError(const QString &txHash, const QString &errMsg)
{
   logger_->error("broadcastZC({}) error: {}", txHash.toStdString(), errMsg.toStdString());
}

bool WalletsManager::estimatedFeePerByte(unsigned int blocksToWait, std::function<void(float)> cb) const
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

   const auto &cbFee = [this, blocksToWait, blocks, cb](float fee) {
      fee *= BTCNumericTypes::BalanceDivider / 1000.0;
      if (fee != 0) {
         if (fee < 5) {
            fee = 5;
         }
         feePerByte_[blocks] = fee;
         lastFeePerByte_[blocks] = QDateTime::currentDateTime();
         cb(fee);
         return;
      }

      if (blocks > 3) {
         feePerByte_[blocks] = 50;
      }
      else if (blocks >= 2) {
         feePerByte_[blocks] = 100;
      }
      cb(feePerByte_[blocks]);
   };
   armory_->estimateFee(blocks, cbFee);
   return true;
}

bool WalletsManager::getNewTransactions() const
{
   if (!armory_) {
      return false;
   }
   auto result = new std::vector<ClientClasses::LedgerEntry>;
   auto walletIds = new std::unordered_set<std::string>;
   const auto &cb = [this, result, walletIds](const bs::Wallet *wallet
      , std::vector<ClientClasses::LedgerEntry> entries) {
      walletIds->erase(wallet->GetWalletId());
      result->insert(result->end(), entries.begin(), entries.end());
      if (walletIds->empty()) {
         delete walletIds;
         emit newTransactions(*result);
         delete result;
      }
   };
   {
      for (const auto &wallet : wallets_) {
         if (armory_->state() != ArmoryConnection::State::Ready) {
            return false;
         }
         if (wallet.second->getHistoryPage(0, cb)) {
            walletIds->insert(wallet.second->GetWalletId());
         }
         else {
            return false;
         }
      }
   }
   if (settlementWallet_ && (armory_->state() == ArmoryConnection::State::Ready)) {
      if (settlementWallet_->getHistoryPage(0, cb)) {
         walletIds->insert(settlementWallet_->GetWalletId());
      }
      else {
         return false;
      }
   }
   return true;
}

std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>> WalletsManager::GetAddressesInAllWallets() const
{
   std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>> result;
   for (const auto &wallet : wallets_) {
      if (!wallet.second->GetUsedAddressCount()) {
         result.push_back({ wallet.second, {} });
      }
      else {
         for (const auto &addr : wallet.second->GetUsedAddressList()) {
            result.push_back({ wallet.second, addr });
         }
      }
   }
   return result;
}

void WalletsManager::ResumeRescan()
{
   std::unordered_map<std::string, std::shared_ptr<bs::hd::Wallet>> rootWallets;
   for (const auto &resumeIdx : appSettings_->UnfinishedWalletsRescan()) {
      const auto &rootWallet = GetHDRootForLeaf(resumeIdx.first);
      if (!rootWallet) {
         continue;
      }
      rootWallets[rootWallet->getWalletId()] = rootWallet;
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
   logger_->debug("Resuming blockchain rescan for {} root wallet[s]", rootWallets.size());
   for (const auto &rootWallet : rootWallets) {
      if (rootWallet.second->startRescan(nullptr, cbr, cbw)) {
         emit walletImportStarted(rootWallet.first);
      }
      else {
         logger_->warn("Rescan for {} is already in progress", rootWallet.second->getName());
      }
   }
}
