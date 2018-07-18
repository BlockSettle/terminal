#include "RootWalletPropertiesDialog.h"
#include "ui_WalletPropertiesDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardPaths>

#include <bech32/ref/c++/segwit_addr.h>

#include "Address.h"
#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "ChangeWalletPasswordDialog.h"
#include "EnterWalletPassword.h"
#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "MessageBoxQuestion.h"
#include "MessageBoxSuccess.h"
#include "MessageBoxWarning.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "WalletDeleteDialog.h"
#include "WalletsManager.h"
#include "WalletsViewModel.h"
#include "WalletsWidget.h"

#include <QSortFilterProxyModel>
#include <QDebug>

class CurrentWalletFilter : public QSortFilterProxyModel
{
public:
   CurrentWalletFilter(const std::shared_ptr<bs::hd::Wallet> &wallet, QObject* parent)
      : QSortFilterProxyModel(parent)
      , wallet_(wallet)
   {
   }

   bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
   {
      if (sourceParent.isValid()) {
         return true;
      }
      auto index = sourceModel()->index(sourceRow, 0, sourceParent);
      auto node = dynamic_cast<WalletsViewModel*>(sourceModel())->getNode(index);

      auto wallet = node->hdWallet();
      return (wallet != nullptr) && (node->hdWallet()->getWalletId() == wallet_->getWalletId());
   }

private:
   std::shared_ptr<bs::hd::Wallet> wallet_;
};

RootWalletPropertiesDialog::RootWalletPropertiesDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
   , const std::shared_ptr<WalletsManager> &walletsManager
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SignContainer> &container
   , WalletsViewModel *walletsModel
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<AssetManager> &assetMgr
   , QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::WalletPropertiesDialog())
  , wallet_(wallet)
  , walletsManager_(walletsManager)
  , signingContainer_(container)
  , appSettings_(appSettings)
  , assetMgr_(assetMgr)
{
   ui_->setupUi(this);

   walletFilter_ = new CurrentWalletFilter(wallet, this);
   walletFilter_->setSourceModel(walletsModel);
   ui_->treeViewWallets->setModel(walletFilter_);

   ui_->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnDescription));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnState));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnSpendableBalance));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnUnconfirmedBalance));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnNbAddresses));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnEmpty));

   connect(ui_->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RootWalletPropertiesDialog::onWalletSelected);

   connect(ui_->deleteButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onDeleteWallet);
   connect(ui_->backupButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onBackupWallet);
   connect(ui_->exportButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onCreateWoWallet);
   connect(ui_->changePassphraseButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onChangePassword);
   connect(ui_->rescanButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onRescanBlockchain);

   updateWalletDetails(wallet_);

   ui_->rescanButton->setEnabled(armory->state() == ArmoryConnection::State::Ready);
   ui_->changePassphraseButton->setEnabled(false);
   if (!wallet_->isWatchingOnly()) {
      walletEncType_ = wallet_->encryptionType();
      walletEncKey_ = wallet_->encryptionKey();
   }

   if (signingContainer_) {
      if (signingContainer_->isOffline() || signingContainer_->isWalletOffline(wallet->getWalletId())) {
         ui_->backupButton->setEnabled(false);
         ui_->changePassphraseButton->setEnabled(false);
      }
      connect(signingContainer_.get(), &SignContainer::HDWalletInfo, this, &RootWalletPropertiesDialog::onHDWalletInfo);
      connect(signingContainer_.get(), &SignContainer::PasswordChanged, this, &RootWalletPropertiesDialog::onPasswordChanged);
      connect(signingContainer_.get(), &SignContainer::HDLeafCreated, this, &RootWalletPropertiesDialog::onHDLeafCreated);
      infoReqId_ = signingContainer_->GetInfo(wallet_);
   }

   ui_->treeViewWallets->expandAll();
}

void RootWalletPropertiesDialog::onDeleteWallet()
{
   WalletDeleteDialog delDlg(wallet_, walletsManager_, signingContainer_, this);
   if (delDlg.exec() == QDialog::Accepted) {
      close();
   }
}

void RootWalletPropertiesDialog::onBackupWallet()
{
   WalletBackupAndVerify(wallet_, signingContainer_, this);
}

void RootWalletPropertiesDialog::onCreateWoWallet()
{
   if (wallet_->isWatchingOnly()) {
      copyWoWallet();
   } else {
      MessageBoxWarning(tr("Create W/O wallet")
         , tr("Watching-only wallet from full wallet should be created on signer side")).exec();
   }
}

void RootWalletPropertiesDialog::copyWoWallet()
{
   const auto dir = QFileDialog::getExistingDirectory(this, tr("Watching-Only Wallet Target Directory")
      , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), QFileDialog::ShowDirsOnly);
   if (dir.isEmpty()) {
      return;
   }
   const auto title = tr("Watching-Only Wallet");
   const auto walletFileName = wallet_->fileNamePrefix(true) + wallet_->getWalletId() + "_wallet.lmdb";
   const auto target = dir + QString::fromStdString("/" + walletFileName);
   if (QFile::exists(target)) {
      MessageBoxQuestion request(title
         , tr("Confirm wallet file overwrite")
         , tr("Wallet file <b>%1</b> already exists in %2. Overwrite it?").arg(QString::fromStdString(walletFileName)).arg(dir)
         , this);
      if (request.exec() == QDialog::Accepted) {
         return;
      }
      QFile::remove(target);
   }

   if (QFile::copy(appSettings_->GetHomeDir() + QString::fromStdString("/" + walletFileName), target)) {
      MessageBoxSuccess(title, tr("Wallet created")
         , tr("Created wallet file <b>%1</b> in <span>%2</span>")
            .arg(QString::fromStdString(walletFileName))
            .arg(dir)
         , this).exec();
   } else {
      MessageBoxCritical(title
         , tr("Failed to copy")
         , tr("Failed to copy <b>%1</b> from %2 to %3")
            .arg(QString::fromStdString(walletFileName)).arg(appSettings_->GetHomeDir())
            .arg(dir)
         , this).exec();
   }
}

void RootWalletPropertiesDialog::onChangePassword()
{
   ChangeWalletPasswordDialog changePasswordDialog(wallet_
      , walletEncType_, walletEncKey_, this);

   if (changePasswordDialog.exec() != QDialog::Accepted) {
      return;
   }

   const auto oldPassword = changePasswordDialog.GetOldPassword();
   const auto newPassword = changePasswordDialog.GetNewPassword();

   if (wallet_->isWatchingOnly()) {
      signingContainer_->ChangePassword(wallet_, newPassword, oldPassword
         , changePasswordDialog.GetNewEncryptionType(), changePasswordDialog.GetNewEncryptionKey());
   }
   else {
      if (wallet_->changePassword(newPassword, oldPassword, changePasswordDialog.GetNewEncryptionType()
         , changePasswordDialog.GetNewEncryptionKey())) {
         MessageBoxSuccess message(tr("Password change")
            , tr("Wallet password successfully changed - don't forget your new password!")
            , this);
         message.exec();
      }
      else {
         MessageBoxCritical message(tr("Password change failure")
            , tr("A problem occured when changing wallet password")
            , this);
         message.exec();
      }
   }
}

void RootWalletPropertiesDialog::onPasswordChanged(const std::string &walletId, bool ok)
{
   if (walletId != wallet_->getWalletId()) {
      return;
   }
   if (ok) {
      MessageBoxSuccess(tr("Password change")
         , tr("Wallet password successfully changed - don't forget your new password!")
         , this).exec();
   }
   else {
      MessageBoxCritical(tr("Password change failure")
         , tr("A problem occured when changing wallet password")
         , this).exec();
   }
}

void RootWalletPropertiesDialog::onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType encType
   , const SecureBinaryData &encKey)
{
   if (!infoReqId_ || (id != infoReqId_)) {
      return;
   }
   infoReqId_ = 0;
   walletEncType_ = encType;
   walletEncKey_ = encKey;
   ui_->changePassphraseButton->setEnabled(true);
}

void RootWalletPropertiesDialog::onWalletSelected()
{
   auto selection = ui_->treeViewWallets->selectionModel()->selectedIndexes();
      auto index = selection[0];

      auto modelIndex = walletFilter_->mapToSource(index);
      auto node = dynamic_cast<WalletsViewModel*>(walletFilter_->sourceModel())->getNode(modelIndex);
      auto wallet = node->hdWallet();

      if (wallet != nullptr) {
         updateWalletDetails(wallet);
      } else {
         const auto wallets = node->wallets();
         if (wallets.size() == 1) {
            updateWalletDetails(wallets[0]);
         } else {
            updateWalletDetails(wallet_);
         }
      }
}

void RootWalletPropertiesDialog::updateWalletDetails(const std::shared_ptr<bs::hd::Wallet>& wallet)
{
   ui_->labelWalletId->setText(QString::fromStdString(wallet->getWalletId()));
   ui_->labelWalletName->setText(QString::fromStdString(wallet->getName()));
   ui_->labelDescription->setText(QString::fromStdString(wallet->getDesc()));

   ui_->balanceWidget->hide();

   ui_->labelAddresses->setText(tr("Groups/Leaves:"));
   ui_->labelAddressesUsed->setText(tr("%1/%2").arg(QString::number(wallet->getNumGroups())).arg(QString::number(wallet->getNumLeaves())));
}

void RootWalletPropertiesDialog::updateWalletDetails(const std::shared_ptr<bs::Wallet>& wallet)
{
   ui_->labelWalletId->setText(QString::fromStdString(wallet->GetWalletId()));
   ui_->labelWalletName->setText(QString::fromStdString(wallet->GetWalletName()));
   ui_->labelDescription->setText(QString::fromStdString(wallet->GetWalletDescription()));

   ui_->labelAddresses->setText(tr("Addresses Used"));
   ui_->labelAddressesUsed->setText(QString::number(wallet->GetUsedAddressCount()));

   if (wallet->isBalanceAvailable()) {
      ui_->labelSpendable->setText(UiUtils::displayAmount(wallet->GetSpendableBalance()));
      ui_->labelUnconfirmed->setText(UiUtils::displayAmount(wallet->GetUnconfirmedBalance()));
      ui_->labelTotal->setText(UiUtils::displayAmount(wallet->GetTotalBalance()));
      ui_->balanceWidget->show();
   } else {
      ui_->balanceWidget->hide();
   }
}

void RootWalletPropertiesDialog::startWalletScan()
{
   const auto walletsMgr = walletsManager_;
   const auto &settings = appSettings_;

   const auto &cbr = [walletsMgr](const std::string &walletId) -> unsigned int {
      const auto &wallet = walletsMgr->GetWalletById(walletId);
      return wallet ? wallet->GetUsedAddressCount() : 0;
   };
   const auto &cbw = [settings](const std::string &walletId, unsigned int idx) {
      settings->SetWalletScanIndex(walletId, idx);
   };

   if (wallet_->startRescan(nullptr, cbr, cbw)) {
      emit walletsManager_->walletImportStarted(wallet_->getWalletId());
   }
   else {
      MessageBoxWarning(tr("Wallet rescan"), tr("Wallet blockchain rescan is already in progress"), this).exec();
   }
   accept();
}

void RootWalletPropertiesDialog::onRescanBlockchain()
{
   ui_->buttonBar->setEnabled(false);

   if (wallet_->isPrimary()) {
      for (const auto &cc : assetMgr_->privateShares(true)) {
         bs::hd::Path path;
         path.append(bs::hd::purpose, true);
         path.append(bs::hd::CoinType::BlockSettle_CC, true);
         path.append(cc, true);
         const auto reqId = signingContainer_->CreateHDLeaf(wallet_, path);
         if (reqId) {
            createCCWalletReqs_[reqId] = cc;
         }
      }
   }
   else {
      startWalletScan();
   }
}

void RootWalletPropertiesDialog::onHDLeafCreated(unsigned int id, BinaryData pubKey, BinaryData chainCode, std::string walletId)
{
   if (!createCCWalletReqs_.empty() && (createCCWalletReqs_.find(id) != createCCWalletReqs_.end())) {
      const auto cc = createCCWalletReqs_[id];
      createCCWalletReqs_.erase(id);

      const auto leafNode = std::make_shared<bs::hd::Node>(pubKey, chainCode, wallet_->networkType());
      const auto group = wallet_->createGroup(bs::hd::CoinType::BlockSettle_CC);
      group->createLeaf(bs::hd::Path::keyToElem(cc), leafNode);

      if (createCCWalletReqs_.empty()) {
         startWalletScan();
      }
   }
}
