#include "WalletDeleteDialog.h"
#include "ui_WalletDeleteDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "MessageBoxSuccess.h"
#include "WalletsWidget.h"
#include "SignContainer.h"


WalletDeleteDialog::WalletDeleteDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
   , const std::shared_ptr<WalletsManager> &walletsMgr, const std::shared_ptr<SignContainer> &container, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletDeleteDialog)
   , hdWallet_(wallet)
   , walletsManager_(walletsMgr)
   , signingContainer_(container)
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
   , const std::shared_ptr<WalletsManager> &walletsMgr, const std::shared_ptr<SignContainer> &container, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletDeleteDialog)
   , wallet_(wallet)
   , walletsManager_(walletsMgr)
   , signingContainer_(container)
{
   init();

   ui_->checkBoxBackup->setChecked(false);
   ui_->checkBoxBackup->hide();

   if (signingContainer_->isOffline() || signingContainer_->isWalletOffline(wallet_->GetWalletId())
      || (walletsManager_->GetSettlementWallet() == wallet_)) {
      ui_->checkBoxDeleteSigner->hide();
   }
}

void WalletDeleteDialog::init()
{
   ui_->setupUi(this);

   ui_->pushButtonOk->setEnabled(false);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &WalletDeleteDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &WalletDeleteDialog::doDelete);
   connect(ui_->checkBoxConfirm, &QCheckBox::clicked, this, &WalletDeleteDialog::onConfirmClicked);
}

void WalletDeleteDialog::deleteHDWallet()
{
   if (ui_->checkBoxBackup->isChecked()) {
      if (!WalletBackupAndVerify(hdWallet_, signingContainer_, this)) {
         MessageBoxCritical(tr("No backup"), tr("No backup was created for this wallet - deletion cancelled")).exec();
         reject();
         return;
      }
   }
   if (ui_->checkBoxDeleteSigner->isChecked()) {
      signingContainer_->DeleteHD(hdWallet_);
   }
   if (walletsManager_->DeleteWalletFile(hdWallet_)) {
      MessageBoxSuccess(tr("Wallet deleted")
         , tr("HD Wallet was successfully deleted")
         , tr("HD wallet \"%1\" (%2) was successfully deleted")
         .arg(QString::fromStdString(hdWallet_->getName()))
         .arg(QString::fromStdString(hdWallet_->getWalletId()))).exec();
      accept();
   }
   else {
      MessageBoxCritical(tr("Wallet deletion failed")
         , tr("Failed to delete local copy of %1").arg(QString::fromStdString(hdWallet_->getName()))).exec();
      reject();
   }
}

void WalletDeleteDialog::deleteWallet()
{
   if (ui_->checkBoxDeleteSigner->isChecked()) {
      signingContainer_->DeleteHD(wallet_);
   }
   if (walletsManager_->DeleteWalletFile(wallet_)) {
      MessageBoxSuccess(tr("Wallet deleted")
         , tr("Wallet was successfully deleted")
         , tr("Wallet \"%1\" (%2) was successfully deleted")
         .arg(QString::fromStdString(wallet_->GetWalletName()))
         .arg(QString::fromStdString(wallet_->GetWalletId()))).exec();
      accept();
   }
   else {
      MessageBoxCritical(tr("Wallet deletion failed")
         , tr("Failed to delete wallet %1").arg(QString::fromStdString(wallet_->GetWalletName()))).exec();
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
