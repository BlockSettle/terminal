#include "CreateWalletDialog.h"
#include "ui_CreateWalletDialog.h"

#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "SignContainer.h"
#include "WalletsManager.h"

CreateWalletDialog::CreateWalletDialog(const std::shared_ptr<WalletsManager>& walletsManager
      , const std::shared_ptr<SignContainer> &container, NetworkType netType, const QString &walletsPath
      , bool createPrimary, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::CreateWalletDialog)
   , walletsManager_(walletsManager)
   , signingContainer_(container)
   , netType_(netType)
   , walletsPath_(walletsPath)
{
   ui_->setupUi(this);

   ui_->checkBoxPrimaryWallet->setEnabled(!walletsManager->HasPrimaryWallet());

   if (createPrimary && !walletsManager->HasPrimaryWallet()) {
      setWindowTitle(tr("Create Primary Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(true);

      ui_->lineEditWalletName->setText(tr("Primary Wallet"));
   } else {
      setWindowTitle(tr("Create New Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(false);

      ui_->lineEditWalletName->setText(tr("Wallet #%1").arg(walletsManager->GetWalletsCount() + 1));
   }

   connect(ui_->lineEditWalletName, &QLineEdit::textChanged, this, &CreateWalletDialog::UpdateAcceptButtonState);
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &CreateWalletDialog::UpdateAcceptButtonState);
   connect(ui_->lineEditPasswordConfirm, &QLineEdit::textChanged, this, &CreateWalletDialog::UpdateAcceptButtonState);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->lineEditDescription, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateWalletDialog::reject);

   UpdateAcceptButtonState();

   connect(signingContainer_.get(), &SignContainer::HDWalletCreated, this, &CreateWalletDialog::onWalletCreated);
   connect(signingContainer_.get(), &SignContainer::Error, this, &CreateWalletDialog::onWalletCreateError);
}

void CreateWalletDialog::showEvent(QShowEvent *event)
{
   ui_->labelHintPrimary->setVisible(ui_->checkBoxPrimaryWallet->isVisible());
   QDialog::showEvent(event);
}

bool CreateWalletDialog::couldCreateWallet() const
{
   return !ui_->lineEditWalletName->text().isEmpty()
         && !ui_->lineEditPassword->text().isEmpty()
         && (ui_->lineEditPassword->text() == ui_->lineEditPasswordConfirm->text())
         && !walletsManager_->WalletNameExists(ui_->lineEditWalletName->text().toStdString());
}

void CreateWalletDialog::UpdateAcceptButtonState()
{
   ui_->pushButtonCreate->setEnabled(couldCreateWallet());
}

void CreateWalletDialog::CreateWallet()
{
   if (!couldCreateWallet()) {
      return;
   }

   ui_->pushButtonCreate->setEnabled(false);

   auto description = ui_->lineEditDescription->text();

   createReqId_ = signingContainer_->CreateHDWallet(ui_->lineEditWalletName->text().toStdString()
      , description.toStdString(), ui_->lineEditPassword->text().toStdString()
      , ui_->checkBoxPrimaryWallet->isChecked(), bs::wallet::Seed(netType_));
}

void CreateWalletDialog::onWalletCreateError(unsigned int id, std::string errMsg)
{
   if (!createReqId_ || (createReqId_ != id)) {
      return;
   }
   createReqId_ = 0;
   MessageBoxCritical info(tr("Create failed")
      , tr("Failed to create or import wallet %1").arg(ui_->lineEditWalletName->text())
      , QString::fromStdString(errMsg), this);

   info.exec();
   reject();
}

void CreateWalletDialog::onWalletCreated(unsigned int id, std::shared_ptr<bs::hd::Wallet> wallet)
{
   if (!createReqId_ || (createReqId_ != id)) {
      return;
   }
   createReqId_ = 0;
   walletsManager_->AdoptNewWallet(wallet, walletsPath_);
   walletId_ = wallet->getWalletId();
   walletCreated_ = true;
   createdAsPrimary_ = ui_->checkBoxPrimaryWallet->isChecked();
   accept();
}
