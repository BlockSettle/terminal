#include "WalletsManager.h"

#include "ApplicationSettings.h"
#include "HDWallet.h"

#include <QDir>
#include <QMutexLocker>

#include <spdlog/spdlog.h>
#include <btc/ecc.h>


class WalletsManagerBlockListener : public PyBlockDataListener
{
public:
   WalletsManagerBlockListener(WalletsManager* walletsManager)
      : walletsManager_(walletsManager)
   {}

   ~WalletsManagerBlockListener() noexcept override = default;

   void StateChanged(PyBlockDataManagerState newState) override {
      if (newState == PyBlockDataManagerState::Ready) {
         walletsManager_->Ready();
      }
   }

   void OnNewBlock(uint32_t) override {
      walletsManager_->BlocksLoaded();
   }

   void OnRefresh() override {
      walletsManager_->BlocksLoaded();
   }

   void ProgressUpdated(BDMPhase , const vector<string> &
      , float , unsigned , unsigned ) override
   {}

private:
   WalletsManager *walletsManager_;
};

WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<ApplicationSettings>& appSettings
 , const std::shared_ptr<PyBlockDataManager>& bdm, bool preferWatchinOnly)
   : appSettings_(appSettings)
   , logger_(logger)
   , bdm_(bdm)
   , preferWatchingOnly_(preferWatchinOnly)
{
   btc_ecc_start();

   networkType_ = appSettings_->get<NetworkType>(ApplicationSettings::netType);
   walletsPath_ = appSettings_->GetHomeDir().toStdString();
   backupPath_ = appSettings_->GetBackupDir().toStdString();
   nbBackupFilesToKeep_ = appSettings_->get<unsigned int>(ApplicationSettings::nbBackupFilesKeep);
   listener_ = std::make_shared<WalletsManagerBlockListener>(this);
   if (bdm_) {
      bdm_->addListener(listener_.get());
      connect(bdm_.get(), &PyBlockDataManager::zeroConfReceived, this, &WalletsManager::onZeroConfReceived, Qt::QueuedConnection);
      connect(bdm_.get(), &PyBlockDataManager::txBroadcastError, this, &WalletsManager::onBroadcastZCError, Qt::QueuedConnection);
   }
}

WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger> &logger, NetworkType netType, const std::string &walletsDir)
   : logger_(logger), networkType_(netType), walletsPath_(walletsDir), preferWatchingOnly_(false)
{
   backupPath_ = walletsPath_ + "/../backup";
   btc_ecc_start();
}

WalletsManager::~WalletsManager()
{
   if (bdm_) {
      bdm_->removeListener(listener_.get());
   }
   btc_ecc_stop();
}

void WalletsManager::Reset(NetworkType netType, const std::string &newWalletsDir)
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

   networkType_ = netType;
   if (!newWalletsDir.empty()) {
      walletsPath_ = newWalletsDir;
   }
   emit walletChanged();
}

void WalletsManager::LoadWallets(const load_progress_delegate& progressDelegate, QString userWalletsDir)
{
   if (!userWalletsDir.isEmpty()) {
      walletsPath_ = userWalletsDir.toStdString();
   }

   logger_->debug("[WalletsManager::LoadWallets] loading wallets from {}"
      , walletsPath_);

   QDir walletsDir(QString::fromStdString(walletsPath_));
   if (!walletsDir.exists()) {
      logger_->debug("Creating wallets path {}", walletsPath_);
      walletsDir.mkpath(QString::fromStdString(walletsPath_));
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
         if (GetNetworkType() != wallet->networkType()) {
            emit error(errorTitle, tr("Wallet %1 (from file %2) has incompatible network type")
               .arg(QString::fromStdString(wallet->getName())).arg(file));
            continue;
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
      if (bs::SettlementWallet::exists(walletsPath_, GetNetworkType())) {
         logger_->debug("Loading settlement wallet");
         settlementWallet_ = bs::SettlementWallet::loadFromFolder(walletsPath_, GetNetworkType());
         connect(settlementWallet_.get(), &bs::SettlementWallet::walletReady, this, &WalletsManager::onWalletReady);
      }
   }
   catch (const WalletException &e) {
      logger_->error("Failed to load settlement wallet: {}", e.what());
      emit error(errorTitle, tr("Failed to load settlement wallet: %1").arg(QLatin1String(e.what())));
   }
   emit walletsLoaded();
}

void WalletsManager::BackupWallet(const hd_wallet_type &wallet) const
{
   if (wallet->isWatchingOnly()) {
      logger_->info("No need to backup watching-only wallet {}", wallet->getName());
      return;
   }
   QString backupDir = QString::fromStdString(backupPath_);
   QDir dirBackup(backupDir);
   if (!dirBackup.exists()) {
      if (!dirBackup.mkpath(backupDir)) {
         logger_->error("Failed to create backup directory {}", backupDir.toStdString());
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
   const auto backupFile = backupDir.toStdString() + "/" + bs::hd::Wallet::fileNamePrefix(false)
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

bool WalletsManager::CreateSettlementWallet()
{
   logger_->debug("Creating settlement wallet");
   try {
      settlementWallet_ = bs::SettlementWallet::create(walletsPath_, GetNetworkType());
   }
   catch (const std::exception &e) {
      logger_->error("Failed to create Settlement wallet: {}", e.what());
   }
   if (settlementWallet_ != nullptr) {
      connect(settlementWallet_.get(), &bs::SettlementWallet::walletReady, this, &WalletsManager::onWalletReady);
      if (bdm_) {
         settlementWallet_->RegisterWallet(bdm_);
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
      if (bdm_) {
         leaf->RegisterWallet(bdm_);
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
   if (!bdm_ || (bdm_->GetState() != PyBlockDataManagerState::Ready)) {
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
   if (!bdm_ || (bdm_->GetState() != PyBlockDataManagerState::Ready)) {
      return 0;
   }
   // TODO: make it lazy init
   BTCNumericTypes::balance_type totalUnconfirmed = 0;

   QMutexLocker lock(&mtxWallets_);
   for (const auto& it : wallets_) {
      totalUnconfirmed += it.second->GetUnconfirmedBalance();
   }

   return totalUnconfirmed;
}

BTCNumericTypes::balance_type WalletsManager::GetTotalBalance() const
{
   if (!bdm_ || (bdm_->GetState() != PyBlockDataManagerState::Ready)) {
      return 0;
   }
   // TODO: make it lazy init
   BTCNumericTypes::balance_type totalBalance = 0;

   QMutexLocker lock(&mtxWallets_);
   for (const auto& it : wallets_) {
      totalBalance += it.second->GetTotalBalance();
   }

   return totalBalance;
}

NetworkType WalletsManager::GetNetworkType() const
{
   return networkType_;
}

void WalletsManager::BlocksLoaded()
{
   logger_->debug("[WalletsManager] Blocks loaded");
   UpdateSavedWallets();
   emit blockchainEvent();
}

void WalletsManager::Ready()
{
   logger_->debug("[WalletsManager] DB ready");
   ResumeRescan();
   UpdateSavedWallets();
   emit walletsReady();
}

void WalletsManager::onWalletReady(const QString &walletId)
{
   emit walletReady(walletId);
   if (!bdm_) {
      return;
   }
   readyWallets_.insert(walletId);
   auto nbWallets = wallets_.size();
   if (settlementWallet_ != nullptr) {
      nbWallets++;
   }
   if (readyWallets_.size() >= nbWallets) {     
      bool rc = bdm_->goOnline();
      logger_->debug("All wallets are ready - going online = {}", rc);
      readyWallets_.clear();
   }
}

bool WalletsManager::IsBalanceLoaded() const
{
   return (bdm_ && (bdm_->GetState() == PyBlockDataManagerState::Ready));
}

WalletsManager::wallet_gen_type WalletsManager::GetWalletById(const std::string& walletId) const
{
   {
      QMutexLocker lock(&mtxWallets_);
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
      QMutexLocker lock(&mtxWallets_);
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
   if (!bdm_) {
      return UINT32_MAX;
   }
   return bdm_->GetTopBlockHeight();
}

void WalletsManager::RegisterSavedWallets()
{
   if (!bdm_) {
      return;
   }
   if (empty()) {
      bdm_->goOnline();
      return;
   }
   {
      QMutexLocker lock(&mtxWallets_);
      for (auto &it : wallets_) {
         it.second->RegisterWallet(bdm_);
      }
   }
   if (settlementWallet_) {
       settlementWallet_->RegisterWallet(bdm_);
   }
}

void WalletsManager::UpdateSavedWallets()
{
   {
      QMutexLocker lock(&mtxWallets_);
      for (auto &it : wallets_) {
         it.second->firstInit();
      }
   }
   if (settlementWallet_) {
      settlementWallet_->firstInit();
   }
}

bool WalletsManager::IsTransactionVerified(const LedgerEntryData& item)
{
   return (bdm_ && bdm_->IsTransactionVerified(item));
}

bs::Transaction::Direction WalletsManager::GetTransactionDirection(Tx tx, const std::shared_ptr<bs::Wallet> &wallet)
{
   if (!tx.isInitialized() || !bdm_ || !wallet) {
      return bs::Transaction::Unknown;
   }

   if (wallet->GetType() == bs::wallet::Type::Authentication) {
      return bs::Transaction::Auth;
   }
   else if (wallet->GetType() == bs::wallet::Type::ColorCoin) {
      return bs::Transaction::Delivery;
   }

   bool ourOuts = false;
   bool otherOuts = false;
   bool ourIns = false;
   bool otherIns = false;
   bool ccTx = false;

   std::vector<TxOut> txOuts;
   txOuts.reserve(tx.getNumTxIn());

   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy((int)i);
      OutPoint op = in.getOutPoint();

      Tx prevTx = bdm_->getTxByHash(op.getTxHash());
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
         const auto addrWallet = GetWalletByAddress(prevOut.getScrAddressStr());
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
         return bs::Transaction::PayIn;
      }
      if (txOuts.size() == 1) {
         const auto addr = txOuts[0].getScrAddressStr();
         const auto settlAE = dynamic_pointer_cast<bs::SettlementAddressEntry>(GetSettlementWallet()->getAddressEntryForAddr(addr));
         if (settlAE) {
            const auto signer = bs::PayoutSigner::WhichSignature(tx, 0, settlAE, logger_);
            if (signer == bs::PayoutSigner::SignedBySeller) {
               return bs::Transaction::Revoke;
            }
         }
      }
      return bs::Transaction::PayOut;
   }

   if (ccTx) {
      return bs::Transaction::Payment;
   }

   if (ourOuts && ourIns && !otherOuts && !otherIns) {
      return bs::Transaction::Internal;
   }

   if (!ourIns) {
      return bs::Transaction::Received;
   }

   if (otherOuts) {
      return bs::Transaction::Sent;
   }

   return bs::Transaction::Unknown;
}

QString WalletsManager::GetTransactionMainAddress(const Tx &tx, const std::shared_ptr<bs::Wallet> &wallet, bool isReceiving)
{
   if (!tx.isInitialized() || !bdm_ || !wallet) {
      return QString();
   }

   const bool isSettlement = (wallet->GetType() == bs::wallet::Type::Settlement);

   std::set<BinaryData> addresses;
   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy((int)i);
      const auto addr = bs::Address::fromTxOut(out);
      bool isOurs = (GetWalletByAddress(addr.id()) == wallet);
      if ((isOurs == isReceiving) || (isOurs && isSettlement)) {
         addresses.insert(addr);
      }
   }

   if (addresses.empty()) {
      for (size_t i = 0; i < tx.getNumTxIn(); i++) {
         auto in = tx.getTxInCopy((int)i);
         OutPoint op = in.getOutPoint();
         Tx prevTx = bdm_->getTxByHash(op.getTxHash());
         if (prevTx.isInitialized()) {
            const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy(op.getTxOutIndex()));
            if (GetWalletByAddress(addr) == wallet) {
               addresses.insert(addr);
            }
         }
      }
   }

   switch (addresses.size()) {
   case 0:
      return QString();

   case 1:
      return bs::Address(*(addresses.begin())).display();

   default:
      return tr("%1 output addresses").arg(addresses.size());
   }

   return QString();
}

WalletsManager::hd_wallet_type WalletsManager::CreateWallet(const std::string& name, const std::string& description
   , const std::string &password, bool primary, bs::wallet::Seed seed)
{
   if (preferWatchingOnly_) {
      throw std::runtime_error("Can't create wallets in watching-only mode");
   }

   if (seed.networkType() == NetworkType::Invalid) {
      seed.setNetworkType(GetNetworkType());
   }
   const auto newWallet = std::make_shared<bs::hd::Wallet>(name, description, false, seed);

   if (hdWallets_.find(newWallet->getWalletId()) != hdWallets_.end()) {
      throw std::runtime_error("HD wallet with id " + newWallet->getWalletId() + " already exists");
   }

   newWallet->createStructure();
   if (primary) {
      newWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
   }
   if (!password.empty()) {
      newWallet->changePassword(password);
   }
   AdoptNewWallet(newWallet);
   return newWallet;
}

void WalletsManager::AdoptNewWallet(const hd_wallet_type &wallet)
{
   wallet->saveToDir(walletsPath_);
   SaveWallet(wallet);
   if (bdm_) {
      wallet->RegisterWallet(bdm_);
   }
   emit newWalletAdded(wallet->getWalletId());
   emit walletsReady();
}

void WalletsManager::AddWallet(const hd_wallet_type &wallet)
{
   if (!wallet) {
      return;
   }
   wallet->saveToDir(walletsPath_);
   SaveWallet(wallet);
   if (bdm_) {
      wallet->RegisterWallet(bdm_);
      emit walletsReady();
   }
}

void WalletsManager::onCCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr)
{
   const auto &cc = ccProd.toStdString();
   QMutexLocker lock(&mtxWallets_);
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

void WalletsManager::onZeroConfReceived(const std::vector<LedgerEntryData> &ledger)
{
   unsigned int foundCnt = 0;
   for (const auto led : ledger) {
      auto wallet = GetWalletById(led.getWalletID());
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

float WalletsManager::estimatedFeePerByte(unsigned int blocksToWait) const
{
   if (!bdm_) {
      return 0;
   }
   if (blocksToWait < 2) {
      blocksToWait = 2;
   } else if (blocksToWait > 1008) {
      blocksToWait = 1008;
   }

   if (lastFeePerByte_[blocksToWait].isValid() && (lastFeePerByte_[blocksToWait].secsTo(QDateTime::currentDateTime()) < 30)) {
      return feePerByte_[blocksToWait];
   }

   try {
      auto fee = bdm_->estimateFee(blocksToWait) * BTCNumericTypes::BalanceDivider / 1000.0;
      if (fee != 0) {
         if (fee < 5) {
            fee = 5;
         }
         feePerByte_[blocksToWait] = fee;
         lastFeePerByte_[blocksToWait] = QDateTime::currentDateTime();
         return fee;
      }
   }
   catch (DbErrorMsg &e) {
      logger_->warn("Error when getting estimated fee ({}) - falling back to static values", e.what());
   }

   if (blocksToWait > 3) {
      feePerByte_[blocksToWait] = 50;
   }
   else if (blocksToWait >= 2) {
      feePerByte_[blocksToWait] = 100;
   }

   return feePerByte_[blocksToWait];
}

std::vector<LedgerEntryData> WalletsManager::getTxPage(uint32_t id) const
{
   if (!bdm_) {
      return {};
   }
   std::vector<LedgerEntryData> result;
   {
      QMutexLocker lock(&mtxWallets_);
      for (const auto &wallet : wallets_) {
         if (bdm_->GetState() != PyBlockDataManagerState::Ready) {
            break;
         }
         const auto page = wallet.second->getHistoryPage(id);
         if (!page.empty()) {
            result.insert(result.end(), page.begin(), page.end());
         }
      }
   }
   if (settlementWallet_ && ((bdm_->GetState() == PyBlockDataManagerState::Ready))) {
      const auto page = settlementWallet_->getHistoryPage(id);
      result.insert(result.end(), page.begin(), page.end());
   }

   return result;
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
