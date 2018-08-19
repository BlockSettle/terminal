#include "CreateWalletDialog.h"
#include "ui_CreateWalletDialog.h"

#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "SignContainer.h"
#include "WalletsManager.h"
#include "WalletKeysCreateWidget.h"

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
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { UpdateAcceptButtonState(); });
   ui_->widgetCreateKeys->init(walletId_);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->lineEditDescription, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateWalletDialog::reject);

   connect(signingContainer_.get(), &SignContainer::HDWalletCreated, this, &CreateWalletDialog::onWalletCreated);
   connect(signingContainer_.get(), &SignContainer::Error, this, &CreateWalletDialog::onWalletCreateError);

   UpdateAcceptButtonState();
}

void CreateWalletDialog::showEvent(QShowEvent *event)
{
   ui_->labelHintPrimary->setVisible(ui_->checkBoxPrimaryWallet->isVisible());
   QDialog::showEvent(event);
}

bool CreateWalletDialog::couldCreateWallet() const
{
   return (ui_->widgetCreateKeys->isValid()
         && !walletsManager_->WalletNameExists(ui_->lineEditWalletName->text().toStdString()));
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
   createReqId_ = signingContainer_->CreateHDWallet(name, description
      , ui_->checkBoxPrimaryWallet->isChecked(), walletSeed_, ui_->widgetCreateKeys->keys()
      , ui_->widgetCreateKeys->keyRank());
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

void CreateWalletDialog::reject()
{
   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}
