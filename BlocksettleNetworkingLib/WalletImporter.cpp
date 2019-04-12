#include "WalletImporter.h"
#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "SignContainer.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


WalletImporter::WalletImporter(const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<ArmoryObject> &armory
   , const std::shared_ptr<AssetManager> &assetMgr, const std::shared_ptr<AuthAddressManager> &authMgr
   , const CbScanReadLast &cbr, const CbScanWriteLast &cbw)
   : QObject(nullptr), signingContainer_(container), walletsMgr_(walletsMgr), armory_(armory)
   , assetMgr_(assetMgr), authMgr_(authMgr)
   , cbReadLast_(cbr), cbWriteLast_(cbw)
{
   connect(signingContainer_.get(), &SignContainer::HDWalletCreated, this, &WalletImporter::onHDWalletCreated);
   connect(signingContainer_.get(), &SignContainer::HDLeafCreated, this, &WalletImporter::onHDLeafCreated);
   connect(signingContainer_.get(), &SignContainer::Error, this, &WalletImporter::onHDWalletError);
}

void WalletImporter::onWalletScanComplete(bs::sync::hd::Group *grp, bs::hd::Path::Elem wallet, bool isValid)
{
   if (!rootWallet_) {
      return;
   }
   if (isValid) {
      if (grp && (grp->index() == rootWallet_->getXBTGroupType())) {
/*         const bs::hd::Path::Elem nextWallet = (wallet == UINT32_MAX) ? 0 : wallet + 1;
         bs::hd::Path path;
         path.append(bs::hd::purpose, true);
         path.append(grp->index(), true);
         path.append(nextWallet, true);
         const auto createNextWalletReq = signingContainer_->createHDLeaf(rootWallet_->walletId(), path, pwdData_);
         if (createNextWalletReq) {
            createNextWalletReqs_[createNextWalletReq] = path;
         }*/
      }
   }
   else {
      if (!((grp->index() == rootWallet_->getXBTGroupType()) && (wallet == 0))) {
         const auto leaf = grp->getLeaf(wallet);
         walletsMgr_->deleteWallet(leaf);
         grp->deleteLeaf(wallet);
      }
   }
}

void WalletImporter::onHDWalletCreated(unsigned int id, std::shared_ptr<bs::sync::hd::Wallet> newWallet)
{
   if (!createWalletReq_ || (createWalletReq_ != id)) {
      return;
   }
   createWalletReq_ = 0;

   const auto &ccList = assetMgr_->privateShares();
   rootWallet_ = newWallet;
   rootWallet_->setArmory(armory_);
   walletsMgr_->adoptNewWallet(newWallet);

   pwdData_.resize(keyRank_.first);

   if (rootWallet_->isPrimary()) {
      authMgr_->CreateAuthWallet(pwdData_, false);
   }
   if (!rootWallet_->isPrimary() || ccList.empty()) {
      if (armory_->state() == ArmoryConnection::State::Ready) {
         rootWallet_->startRescan([this](bs::sync::hd::Group *grp, bs::hd::Path::Elem wallet, bool isValid) {
            onWalletScanComplete(grp, wallet, isValid);
         }, cbReadLast_, cbWriteLast_);
      }
      emit walletCreated(rootWallet_->walletId());
   }
   else {
      for (const auto &cc : ccList) {
         bs::hd::Path path;
         path.append(bs::hd::purpose, true);
         path.append(bs::hd::CoinType::BlockSettle_CC, true);
         path.append(cc, true);
         const auto reqId = signingContainer_->createHDLeaf(rootWallet_->walletId(), path, pwdData_);
         if (reqId) {
            createCCWalletReqs_[reqId] = cc;
         }
      }
   }
}

void WalletImporter::onHDLeafCreated(unsigned int id, const std::shared_ptr<bs::sync::hd::Leaf> &leaf)
{
   if (!createCCWalletReqs_.empty() && (createCCWalletReqs_.find(id) != createCCWalletReqs_.end())) {
      const auto cc = createCCWalletReqs_[id];
      createCCWalletReqs_.erase(id);

      const auto group = rootWallet_->createGroup(bs::hd::CoinType::BlockSettle_CC);
      group->addLeaf(leaf);
      leaf->setData(assetMgr_->getCCGenesisAddr(cc).display());
      leaf->setData(assetMgr_->getCCLotSize(cc));

      if (createCCWalletReqs_.empty()) {
         if (armory_->state() == ArmoryConnection::State::Ready) {
            rootWallet_->startRescan([this](bs::sync::hd::Group *grp, bs::hd::Path::Elem wallet, bool isValid) {
               onWalletScanComplete(grp, wallet, isValid);
            }, cbReadLast_, cbWriteLast_);
         }
         emit walletCreated(rootWallet_->walletId());
      }
   }
   else if (!createNextWalletReqs_.empty() && (createNextWalletReqs_.find(id) != createNextWalletReqs_.end())) {
      const auto path = createNextWalletReqs_[id];
      createNextWalletReqs_.erase(id);

      const auto group = rootWallet_->getGroup(static_cast<bs::hd::CoinType>(path.get(-2)));
      group->addLeaf(leaf);
      if (armory_->state() == ArmoryConnection::State::Ready) {
         leaf->setScanCompleteCb([this, group](bs::hd::Path::Elem wlt, bool status) {
            onWalletScanComplete(group.get(), wlt, status);
         });
         leaf->scanAddresses();
      }
   }
}

void WalletImporter::onHDWalletError(unsigned int id, std::string errMsg)
{
   if (!createWalletReq_ || (createWalletReq_ != id)) {
      return;
   }
   createWalletReq_ = 0;
   emit error(QString::fromStdString(errMsg));
}

void WalletImporter::Import(const std::string &name, const std::string &description
   , bs::core::wallet::Seed seed, bool primary, const std::vector<bs::wallet::PasswordData> &pwdData
   , bs::wallet::KeyRank keyRank)
{
   if (!signingContainer_ || signingContainer_->isOffline()) {
      emit error(tr("Can't start import with missing or offline signer"));
      return;
   }
   pwdData_ = pwdData;
   keyRank_ = keyRank;
   createWalletReq_ = signingContainer_->createHDWallet(name, description, primary, seed, pwdData, keyRank);
   if (!createWalletReq_) {
      emit error(tr("Failed to create HD wallet"));
   }
}
