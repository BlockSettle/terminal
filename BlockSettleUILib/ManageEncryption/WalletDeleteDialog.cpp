#include "WalletDeleteDialog.h"
#include "ui_WalletDeleteDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

#include "HDWallet.h"
#include "BSMessageBox.h"
#include "WalletsWidget.h"
#include "SignContainer.h"
#include "WalletBackupDialog.h"


WalletDeleteDialog::WalletDeleteDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
   , const std::shared_ptr<WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &container
   , std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<spdlog::logger> &logger
   , QWidget *parent, bool fixedCheckBoxes, bool delRemote)
   : QDialog(parent)
   , ui_(new Ui::WalletDeleteDialog)
   , hdWallet_(wallet)
   , walletsManager_(walletsMgr)
   , signingContainer_(container)
   , appSettings_(appSettings)
   , logger_(logger)
   , fixedCheckBoxes_(fixedCheckBoxes)
   , delRemoteWallet_(delRemote)
{
   init();

   const auto &group = hdWallet_->getGroup(hdWallet_->getXBTGroupType());
   const auto &xbtLeaf = group ? group->getLeaf(0) : nullptr;
   if (signingContainer_->isOffline() || (xbtLeaf && signingContainer_->isWalletOffline(xbtLeaf->GetWalletId()))) {
      ui_->checkBoxBackup->setChecked(false);
      ui_->checkBoxBackup->hide();
      ui_->checkBoxDeleteSigner->hide();
   }
}

WalletDeleteDialog::WalletDeleteDialog(const std::shared_ptr<bs::Wallet> &wallet
   , const std::shared_ptr<WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &container
   , std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<spdlog::logger> &logger
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletDeleteDialog)
   , wallet_(wallet)
   , walletsManager_(walletsMgr)
   , signingContainer_(container)
   , appSettings_(appSettings)
   , logger_(logger)
   , fixedCheckBoxes_(false), delRemoteWallet_(false)
{
   init();

   ui_->checkBoxBackup->setChecked(false);
   ui_->checkBoxBackup->hide();

   if (signingContainer_->isOffline() || signingContainer_->isWalletOffline(wallet_->GetWalletId())
      || (walletsManager_->GetSettlementWallet() == wallet_)) {
      ui_->checkBoxDeleteSigner->hide();
   }
}

WalletDeleteDialog::~WalletDeleteDialog() = default;

void WalletDeleteDialog::init()
{
   ui_->setupUi(this);

   ui_->checkBoxDeleteSigner->setChecked(delRemoteWallet_);

   if (fixedCheckBoxes_) {
      ui_->checkBoxDeleteSigner->setEnabled(false);
   }

   ui_->pushButtonOk->setEnabled(false);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &WalletDeleteDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &WalletDeleteDialog::doDelete);
   connect(ui_->checkBoxConfirm, &QCheckBox::clicked, this, &WalletDeleteDialog::onConfirmClicked);
}

void WalletDeleteDialog::deleteHDWallet()
{
   if (ui_->checkBoxBackup->isChecked()) {
      if (!WalletBackupAndVerify(hdWallet_, signingContainer_, appSettings_
                                 , logger_, this)) {
//         BSMessageBox(BSMessageBox::critical, tr("No backup")
//            , tr("No backup was created for this wallet - deletion cancelled"), this).exec();
         reject();
         return;
      }
   }
   if (ui_->checkBoxDeleteSigner->isChecked()) {
      signingContainer_->DeleteHDRoot(hdWallet_->getWalletId());
   }
   if (walletsManager_->DeleteWalletFile(hdWallet_)) {
      BSMessageBox(BSMessageBox::success, tr("Wallet deleted")
         , tr("HD Wallet was successfully deleted")
         , tr("HD wallet \"%1\" (%2) was successfully deleted")
         .arg(QString::fromStdString(hdWallet_->getName()))
         .arg(QString::fromStdString(hdWallet_->getWalletId())), this).exec();
      accept();
   }
   else {
      BSMessageBox(BSMessageBox::critical, tr("Wallet deletion failed")
         , tr("Failed to delete local copy of %1").arg(QString::fromStdString(hdWallet_->getName())), this).exec();
      reject();
   }
}

void WalletDeleteDialog::deleteWallet()
{
   if (ui_->checkBoxDeleteSigner->isChecked()) {
      signingContainer_->DeleteHDLeaf(wallet_->GetWalletId());
   }
   if (walletsManager_->DeleteWalletFile(wallet_)) {
      BSMessageBox(BSMessageBox::success, tr("Wallet deleted")
         , tr("Wallet was successfully deleted")
         , tr("Wallet \"%1\" (%2) was successfully deleted")
         .arg(QString::fromStdString(wallet_->GetWalletName()))
         .arg(QString::fromStdString(wallet_->GetWalletId())), this).exec();
      accept();
   }
   else {
      BSMessageBox(BSMessageBox::critical, tr("Wallet deletion failed")
         , tr("Failed to delete wallet %1").arg(QString::fromStdString(wallet_->GetWalletName())), this).exec();
      reject();
   }
}

void WalletDeleteDialog::doDelete()
{
   if (hdWallet_) {
      deleteHDWallet();
   }
   else if (wallet_) {
      deleteWallet();
   }
}

void WalletDeleteDialog::onConfirmClicked()
{
   ui_->pushButtonOk->setEnabled(ui_->checkBoxConfirm->isChecked());
}
