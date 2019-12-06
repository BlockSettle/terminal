/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "disable_warnings.h"
#include <spdlog/spdlog.h>
#include <btc/ecc.h>
#include "enable_warnings.h"
#include <ctime>
#include "BIP150_151.h"
#include "CoreWalletsManager.h"
#include "CoreHDWallet.h"
#include "SystemFileUtils.h"

using namespace bs::core;

static const char *wrongControlPasswordException = "empty passphrase";

WalletsManager::WalletsManager(const std::shared_ptr<spdlog::logger> &logger, unsigned int nbBackups)
   : logger_(logger), nbBackupFilesToKeep_(nbBackups)
{}

void WalletsManager::reset()
{
   hdWallets_.clear();
   walletNames_.clear();
   hdWalletsId_.clear();
//   settlementWallet_.reset();
}

bool WalletsManager::loadWallets(NetworkType netType, const std::string &walletsPath
   , const SecureBinaryData &controlPassphrase, const CbProgress &cbProgress)
{
   if (walletsPath.empty()) {
      return true;
   }
   if (!SystemFileUtils::pathExist(walletsPath)) {
      logger_->debug("Creating wallets path {}", walletsPath);
      SystemFileUtils::mkPath(walletsPath);
   }

   const auto fileList = SystemFileUtils::readDir(walletsPath, "*.lmdb");
   const size_t totalCount = fileList.size();
   size_t current = 0;

   for (const auto &file : fileList) {
      if (!isWalletFile(file)) {
         continue;
      }
      try {
         logger_->debug("Loading BIP44 wallet from {}", file);
         const auto wallet = std::make_shared<hd::Wallet>(file, netType
            , walletsPath, controlPassphrase, logger_);
         current++;
         if (cbProgress) {
            cbProgress(current, totalCount);
         }
         if ((netType != NetworkType::Invalid) && (netType != wallet->networkType())) {
            logger_->warn("[{}] Network type mismatch: loading {}, wallet has {}", __func__, (int)netType, (int)wallet->networkType());
         }
         saveWallet(wallet);
      }
      catch (const std::exception &e) {
         logger_->warn("Failed to load BIP44 wallet: {}", e.what());

         if (!strncmp(e.what(), wrongControlPasswordException, strlen(wrongControlPasswordException))) {
            return false;
         }
      }
   }
   walletsLoaded_ = true;
   return true;
}

WalletsManager::HDWalletPtr WalletsManager::loadWoWallet(NetworkType netType
   , const std::string &walletsPath, const std::string &fileName
   , const SecureBinaryData &controlPassphrase)
{
   if (walletsPath.empty()) {
      return nullptr;
   }
   if (!SystemFileUtils::pathExist(walletsPath)) {
      logger_->debug("Creating wallets path {}", walletsPath);
      SystemFileUtils::mkPath(walletsPath);
   }

   try {
      logger_->debug("Loading BIP44 WO-wallet from {}", fileName);
      const auto wallet = std::make_shared<hd::Wallet>(fileName
         , netType, walletsPath, controlPassphrase, logger_);
      if (!wallet->isWatchingOnly()) {
         logger_->error("Wallet {} is not watching-only", fileName);
         return nullptr;
      }

      saveWallet(wallet);
      return wallet;
   } catch (const std::exception &e) {
      logger_->warn("Failed to load WO-wallet: {}", e.what());
   }
   return nullptr;
}

void WalletsManager::changeControlPassword(const SecureBinaryData &oldPass, const SecureBinaryData &newPass)
{
   for (const auto &hdWallet : hdWallets_) {
      hdWallet.second->changeControlPassword(oldPass, newPass);
   }
}

void WalletsManager::backupWallet(const HDWalletPtr &wallet, const std::string &targetDir) const
{
   if (wallet->isWatchingOnly()) {
      logger_->info("No need to backup watching-only wallet {}", wallet->name());
      return;
   }
   if (!SystemFileUtils::pathExist(targetDir)) {
      if (!SystemFileUtils::mkPath(targetDir)) {
         logger_->error("Failed to create backup directory {}", targetDir);
         return;
      }
   }
   const auto &lockFiles = SystemFileUtils::readDir(targetDir, "*.lmdb-lock");
   for (const auto &file : lockFiles) {
      SystemFileUtils::rmFile(targetDir + "/" + file);
   }
   const auto files = SystemFileUtils::readDir(targetDir
      , hd::Wallet::fileNamePrefix(false) + wallet->walletId() + "_*.lmdb");
   if (!files.empty() && (files.size() >= (int)nbBackupFilesToKeep_)) {
      for (int i = 0; i <= files.size() - (int)nbBackupFilesToKeep_; i++) {
         logger_->debug("Removing old backup file {}", files[i]);
         SystemFileUtils::rmFile(targetDir + "/" + files[i]);
      }
   }
   const auto tm = std::time(nullptr);
   char tmStr[32];
   std::strftime(tmStr, sizeof(tmStr), "%Y%j%H%M%S", std::localtime(&tm));
   const auto backupFile = targetDir + "/" + hd::Wallet::fileNamePrefix(false)
      + wallet->walletId()  + "_" + tmStr + ".lmdb";
   
   wallet->copyToFile(backupFile);
}

bool WalletsManager::isWalletFile(const std::string &fileName) const
{
   return true;
}

WalletsManager::HDWalletPtr WalletsManager::getPrimaryWallet() const
{
   HDWalletPtr primaryWallet = nullptr;
   int count = 0;
   for (const auto &hdWallet : hdWallets_) {
      if (hdWallet.second->isPrimary()) {
         primaryWallet = hdWallet.second;
         count++;
      }
   }
   if (count > 1) {
      throw std::runtime_error("Only one primary wallet allowed");
   }

   return primaryWallet;
}

void WalletsManager::saveWallet(const HDWalletPtr &wallet)
{
   hdWalletsId_.emplace_back(wallet->walletId());
   hdWallets_[wallet->walletId()] = wallet;
   walletNames_.insert(wallet->name());
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

WalletsManager::WalletPtr WalletsManager::getWalletById(const std::string& walletId) const
{
   for (const auto &hdWallet : hdWallets_) {
      auto leafPtr = hdWallet.second->getLeaf(walletId);
      if (leafPtr != nullptr)
         return leafPtr;
   }

/*   if (settlementWallet_ && (settlementWallet_->walletId() == walletId)) {
      return settlementWallet_;
   }*/
   return nullptr;
}

WalletsManager::WalletPtr WalletsManager::getWalletByAddress(const bs::Address &addr) const
{
   for (const auto wallet : hdWallets_)
   {
      for (auto& group : wallet.second->getGroups())
      {
         for (auto& leafPtr : group->getAllLeaves())
         {
            if (leafPtr && (leafPtr->containsAddress(addr) ||
               leafPtr->containsHiddenAddress(addr)))
               return leafPtr;
         }
      }
   }

/*   if ((settlementWallet_ != nullptr) && settlementWallet_->containsAddress(addr))
      return settlementWallet_;
*/
   return nullptr;
}

void WalletsManager::eraseWallet(const WalletPtr &wallet)
{
   /*if (!wallet) {
      return;
   }
   const auto itId = std::find(walletsId_.begin(), walletsId_.end(), wallet->walletId());
   if (itId != walletsId_.end()) {
      walletsId_.erase(itId);
   }
   wallets_.erase(wallet->walletId());*/
}

bool WalletsManager::deleteWalletFile(const WalletPtr &wallet)
{
   bool isHDLeaf = false;
   logger_->info("Removing wallet {} ({})...", wallet->name(), wallet->walletId());
   for (auto hdWallet : hdWallets_) {
      const auto leaves = hdWallet.second->getLeaves();
      if (std::find(leaves.begin(), leaves.end(), wallet) != leaves.end()) {
         for (auto group : hdWallet.second->getGroups()) {
            if (group->deleteLeaf(wallet)) {
               isHDLeaf = true;
               eraseWallet(wallet);
               break;
            }
         }
      }
      if (isHDLeaf) {
         break;
      }
   }

   if (!isHDLeaf) {
      auto filename = wallet->getFilename();
      if (filename.size() > 0)
      {
         wallet->shutdown();
         if (std::remove(filename.c_str()) != 0)
         {
            logger_->error("Failed to remove wallet file for {}", wallet->name());
            return false;
         }
      }

/*      if (wallet == settlementWallet_) {
         settlementWallet_ = nullptr;
      }*/
      eraseWallet(wallet);
   }

   return true;
}

bool WalletsManager::deleteWalletFile(const HDWalletPtr &wallet)
{
   const auto it = hdWallets_.find(wallet->walletId());
   if (it == hdWallets_.end()) {
      logger_->warn("Unknown HD wallet {} ({})", wallet->name(), wallet->walletId());
      return false;
   }

   for (const auto &leaf : wallet->getLeaves()) {
      eraseWallet(leaf);
   }

   const auto itId = std::find(hdWalletsId_.begin(), hdWalletsId_.end(), wallet->walletId());
   if (itId != hdWalletsId_.end()) {
      hdWalletsId_.erase(itId);
   }
   hdWallets_.erase(wallet->walletId());
   walletNames_.erase(wallet->name());

   auto walletID = wallet->walletId();
   const bool result = wallet->eraseFile();
   logger_->info("Wallet {} ({}) removed: {}", wallet->name(), walletID, result);

   return result;
}

WalletsManager::HDWalletPtr WalletsManager::createWallet(
   const std::string& name, const std::string& description
   , wallet::Seed seed, const std::string &folder
   , const bs::wallet::PasswordData &pd, bool primary)
{
   const HDWalletPtr newWallet = std::make_shared<hd::Wallet>(
      name, description, seed, pd, folder, logger_);

   if (hdWallets_.find(newWallet->walletId()) != hdWallets_.end()) {
      throw std::runtime_error("HD wallet with id " + newWallet->walletId() + " already exists");
   }

   {
      const bs::core::WalletPasswordScoped lock(newWallet, pd.password);
      newWallet->createStructure();
      if (primary) {
         newWallet->createChatPrivKey();
         auto group = newWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
         if (!userId_.isNull()) {
            newWallet->createGroup(bs::hd::CoinType::BlockSettle_Settlement);
            const auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
            if (authGroup) {
               authGroup->setSalt(userId_);
               const auto authLeaf = authGroup->createLeaf(AddressEntryType_Default, 0, 5);
               if (authLeaf) {
                  for (const auto &authAddr : authLeaf->getPooledAddressList()) {
                     try {
                        newWallet->createSettlementLeaf(authAddr);
                     }
                     catch (const std::exception &e) {
                        logger_->error("[{}] failed to create settlement leaf for {}: {}"
                           , __func__, authAddr.display(), e.what());
                     }
                  }
               }
               else {
                  logger_->error("[{}] failed to create auth leaf", __func__);
               }
            }
            else {
               logger_->error("[{}] invalid auth group", __func__);
            }
         }

         if (!ccLeaves_.empty()) {
            group = newWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
            for (const auto &cc : ccLeaves_) {
               try {
                  group->createLeaf(AddressEntryType_Default, cc);
               }
               catch (const std::exception &e) {
                  logger_->error("[{}] CC leaf {} creation failed: {}"
                     , __func__, cc, e.what());
               }
            }
         }
      }
   }

   addWallet(newWallet);
   return newWallet;
}

void WalletsManager::addWallet(const HDWalletPtr &wallet)
{
   if (!wallet) {
      return;
   }

   saveWallet(wallet);
}
