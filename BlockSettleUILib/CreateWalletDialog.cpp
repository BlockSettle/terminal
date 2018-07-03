#include "CreateWalletDialog.h"
#include "ui_CreateWalletDialog.h"

#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "SignContainer.h"
#include "WalletsManager.h"

#include <spdlog/spdlog.h>


CreateWalletDialog::CreateWalletDialog(const std::shared_ptr<WalletsManager>& walletsManager
      , const std::shared_ptr<SignContainer> &container, NetworkType netType, const QString &walletsPath
      , bool createPrimary, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::CreateWalletDialog)
   , walletsManager_(walletsManager)
   , signingContainer_(container)
   , walletSeed_(netType, SecureBinaryData().GenerateRandom(32))
   , walletsPath_(walletsPath)
   , frejaSign_(spdlog::get(""))
{
   ui_->setupUi(this);

   walletId_ = bs::hd::Node(walletSeed_).getId();

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
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &CreateWalletDialog::onPasswordChanged);
   connect(ui_->lineEditPasswordConfirm, &QLineEdit::textChanged, this, &CreateWalletDialog::onPasswordChanged);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->lineEditDescription, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateWalletDialog::reject);

   connect(ui_->radioButtonPassword, &QRadioButton::clicked, this, &CreateWalletDialog::onEncTypeChanged);
   connect(ui_->radioButtonFreja, &QRadioButton::clicked, this, &CreateWalletDialog::onEncTypeChanged);
   connect(ui_->lineEditFrejaId, &QLineEdit::textChanged, this, &CreateWalletDialog::onFrejaIdChanged);
   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &CreateWalletDialog::startFrejaSign);

   connect(signingContainer_.get(), &SignContainer::HDWalletCreated, this, &CreateWalletDialog::onWalletCreated);
   connect(signingContainer_.get(), &SignContainer::Error, this, &CreateWalletDialog::onWalletCreateError);

   connect(&frejaSign_, &FrejaSignWallet::succeeded, this, &CreateWalletDialog::onFrejaSucceeded);
   connect(&frejaSign_, &FrejaSign::failed, this, &CreateWalletDialog::onFrejaFailed);
   connect(&frejaSign_, &FrejaSign::statusUpdated, this, &CreateWalletDialog::onFrejaStatusUpdated);

   UpdateAcceptButtonState();
   onEncTypeChanged();
}

void CreateWalletDialog::showEvent(QShowEvent *event)
{
   ui_->labelHintPrimary->setVisible(ui_->checkBoxPrimaryWallet->isVisible());
   QDialog::showEvent(event);
}

bool CreateWalletDialog::couldCreateWallet() const
{
   return !walletPassword_.isNull()
         && !walletsManager_->WalletNameExists(ui_->lineEditWalletName->text().toStdString());
}

void CreateWalletDialog::onPasswordChanged(const QString &)
{
   if (!ui_->lineEditWalletName->text().isEmpty()
      && !ui_->lineEditPassword->text().isEmpty()
      && (ui_->lineEditPassword->text() == ui_->lineEditPasswordConfirm->text())) {
      walletPassword_ = ui_->lineEditPassword->text().toStdString();
   }
   UpdateAcceptButtonState();
}

void CreateWalletDialog::onFrejaIdChanged(const QString &)
{
   ui_->pushButtonFreja->setEnabled(!ui_->lineEditFrejaId->text().isEmpty());
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

   const auto &name = ui_->lineEditWalletName->text().toStdString();
   const auto &description = ui_->lineEditDescription->text().toStdString();
   if (ui_->radioButtonFreja->isChecked()) {
      walletSeed_.setEncryptionKey(ui_->lineEditFrejaId->text().toStdString());
   }

   createReqId_ = signingContainer_->CreateHDWallet(name, description, walletPassword_
      , ui_->checkBoxPrimaryWallet->isChecked(), walletSeed_);
   walletPassword_.clear();
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
   if (walletId_ != wallet->getWalletId()) {
      MessageBoxCritical(tr("Wallet ID mismatch")
         , tr("Pre-created wallet id: %1, id after creation: %2")
            .arg(QString::fromStdString(walletId_)).arg(QString::fromStdString(wallet->getWalletId()))
         , this).exec();
      reject();
   }
   createReqId_ = 0;
   walletsManager_->AdoptNewWallet(wallet, walletsPath_);
   walletCreated_ = true;
   createdAsPrimary_ = ui_->checkBoxPrimaryWallet->isChecked();
   accept();
}

void CreateWalletDialog::onEncTypeChanged()
{
   if (ui_->radioButtonPassword->isChecked()) {
      ui_->widgetFreja->hide();
      ui_->widgetPassword->show();
      ui_->widgetPasswordConfirm->show();
      walletSeed_.setEncryptionType(bs::wallet::EncryptionType::Password);
   }
   else if (ui_->radioButtonFreja->isChecked()) {
      ui_->widgetFreja->show();
      ui_->widgetPassword->hide();
      ui_->widgetPasswordConfirm->hide();
      walletSeed_.setEncryptionType(bs::wallet::EncryptionType::Freja);
   }
}

void CreateWalletDialog::startFrejaSign()
{
   frejaSign_.start(ui_->lineEditFrejaId->text(), tr("New wallet creation"), walletId_);
   ui_->pushButtonFreja->setEnabled(false);
   ui_->lineEditFrejaId->setEnabled(false);
}

void CreateWalletDialog::onFrejaSucceeded(SecureBinaryData password)
{
   ui_->labelFreja->setText(tr("Successfully signed"));
   walletPassword_ = password;
   UpdateAcceptButtonState();
}

void CreateWalletDialog::onFrejaFailed(const QString &text)
{
   ui_->pushButtonFreja->setEnabled(true);
   ui_->labelFreja->setText(tr("Freja failed: %1").arg(text));
}

void CreateWalletDialog::onFrejaStatusUpdated(const QString &status)
{
   ui_->labelFreja->setText(status);
}
