#include "WalletDeleteDialog.h"
#include "ui_WalletDeleteDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

#include "BSMessageBox.h"
#include "WalletsWidget.h"
#include "SignContainer.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncSettlementWallet.h"
#include "Wallets/SyncWalletsManager.h"


WalletDeleteDialog::WalletDeleteDialog(const std::shared_ptr<bs::sync::hd::Wallet> &wallet
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &container
   , std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<spdlog::logger> &logger
   , QWidget *parent, bool fixedCheckBoxes, bool delRemote)
   : QDialog(parent)
   , ui_(new Ui::WalletDeleteDialog)
   , hdWallet_(wallet)
   , walletsManager_(walletsMgr)
   , signingContainer_(container)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
   , logger_(logger)
   , fixedCheckBoxes_(fixedCheckBoxes)
   , delRemoteWallet_(delRemote)
{
   init();

   const auto &group = hdWallet_->getGroup(hdWallet_->getXBTGroupType());
   const auto &xbtLeaf = group ? group->getLeaf(0) : nullptr;
   if (signingContainer_->isOffline() || (xbtLeaf && signingContainer_->isWalletOffline(xbtLeaf->walletId()))) {
      ui_->checkBoxBackup->setChecked(false);
      ui_->checkBoxBackup->hide();
      ui_->checkBoxDeleteSigner->hide();
   }
}

WalletDeleteDialog::WalletDeleteDialog(const std::shared_ptr<bs::sync::Wallet> &wallet
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &container
   , std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<spdlog::logger> &logger
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletDeleteDialog)
   , wallet_(wallet)
   , walletsManager_(walletsMgr)
   , signingContainer_(container)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
   , logger_(logger)
   , fixedCheckBoxes_(false), delRemoteWallet_(false)
{
   init();

   ui_->checkBoxBackup->setChecked(false);
   ui_->checkBoxBackup->hide();

   if (signingContainer_->isOffline() || signingContainer_->isWalletOffline(wallet_->walletId())
      || (walletsManager_->getSettlementWallet()->walletId() == wallet_->walletId())) {
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
   if (walletsManager_->deleteWallet(hdWallet_)) {
      BSMessageBox(BSMessageBox::success, tr("Wallet deleted")
         , tr("HD Wallet was successfully deleted")
         , tr("HD wallet \"%1\" (%2) was successfully deleted")
         .arg(QString::fromStdString(hdWallet_->name()))
         .arg(QString::fromStdString(hdWallet_->walletId())), this).exec();
      accept();
   }
   else {
      BSMessageBox(BSMessageBox::critical, tr("Wallet deletion failed")
         , tr("Failed to delete wallet %1").arg(QString::fromStdString(hdWallet_->name())), this).exec();
      reject();
   }
}

void WalletDeleteDialog::deleteWallet()
{
   if (walletsManager_->deleteWallet(wallet_)) {
      BSMessageBox(BSMessageBox::success, tr("Wallet deleted")
         , tr("Wallet was successfully deleted")
         , tr("Wallet \"%1\" (%2) was successfully deleted")
         .arg(QString::fromStdString(wallet_->name()))
         .arg(QString::fromStdString(wallet_->walletId())), this).exec();
      accept();
   }
   else {
      BSMessageBox(BSMessageBox::critical, tr("Wallet deletion failed")
         , tr("Failed to delete wallet %1").arg(QString::fromStdString(wallet_->name())), this).exec();
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
