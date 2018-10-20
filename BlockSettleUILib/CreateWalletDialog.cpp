#include "CreateWalletDialog.h"
#include "ui_CreateWalletDialog.h"

#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "WalletPasswordVerifyDialog.h"
#include "NewWalletSeedDialog.h"
#include "SignContainer.h"
#include "EnterWalletPassword.h"
#include "WalletsManager.h"
#include "WalletKeysCreateWidget.h"
#include "FrejaNotice.h"
#include "UiUtils.h"

#include <spdlog/spdlog.h>


CreateWalletDialog::CreateWalletDialog(const std::shared_ptr<WalletsManager>& walletsManager
   , const std::shared_ptr<SignContainer> &container
   , const QString &walletsPath
   , const bs::wallet::Seed& walletSeed
   , const std::string& walletId
   , bool createPrimary
   , const QString& username
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget *parent)

   : QDialog(parent)
   , ui_(new Ui::CreateWalletDialog)
   , walletsManager_(walletsManager)
   , signingContainer_(container)
   , appSettings_(appSettings)
   , walletSeed_(walletSeed)
   , walletId_(walletId)
   , walletsPath_(walletsPath)
{
   ui_->setupUi(this);

   ui_->checkBoxPrimaryWallet->setEnabled(!walletsManager->HasPrimaryWallet());
   ui_->checkBoxPrimaryWallet->setChecked(!walletsManager->HasPrimaryWallet());

   if (createPrimary && !walletsManager->HasPrimaryWallet()) {
      setWindowTitle(tr("Create Primary Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(true);

      ui_->lineEditWalletName->setText(tr("Primary Wallet"));
   } else {
      setWindowTitle(tr("Create New Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(false);

      ui_->lineEditWalletName->setText(tr("Wallet #%1").arg(walletsManager->GetWalletsCount() + 1));
   }

   ui_->lineEditDescription->setValidator(new UiUtils::WalletDescriptionValidator(this));

   ui_->labelWalletId->setText(QString::fromStdString(walletId_));

   connect(ui_->lineEditWalletName, &QLineEdit::textChanged, this, &CreateWalletDialog::updateAcceptButtonState);
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateAcceptButtonState(); });
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyTypeChanged,
      this, &CreateWalletDialog::onKeyTypeChanged);

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideWidgetContol | WalletKeysCreateWidget::HideFrejaConnectButton);
   ui_->widgetCreateKeys->init(MobileClientRequest::ActivateWallet
      , walletId_, username, appSettings);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->lineEditDescription, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateWalletDialog::reject);

   connect(signingContainer_.get(), &SignContainer::HDWalletCreated, this, &CreateWalletDialog::onWalletCreated);
   connect(signingContainer_.get(), &SignContainer::Error, this, &CreateWalletDialog::onWalletCreateError);

   adjustSize();
   setMinimumSize(size());
}

CreateWalletDialog::~CreateWalletDialog() = default;

void CreateWalletDialog::updateAcceptButtonState()
{
   ui_->pushButtonContinue->setEnabled(ui_->widgetCreateKeys->isValid() &&
      !ui_->lineEditWalletName->text().isEmpty());
}

void CreateWalletDialog::CreateWallet()
{
   const QString &walletName = ui_->lineEditWalletName->text();
   const QString  &walletDescription = ui_->lineEditDescription->text();
   std::vector<bs::wallet::PasswordData> keys;

   bool result = checkNewWalletValidity(walletsManager_.get(), walletName, walletId_
      , ui_->widgetCreateKeys, &keys, appSettings_, this);
   if (!result) {
      return;
   }

   ui_->pushButtonContinue->setEnabled(false);

   createReqId_ = signingContainer_->CreateHDWallet(walletName.toStdString(), walletDescription.toStdString()
      , ui_->checkBoxPrimaryWallet->isChecked(), walletSeed_, keys
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

void CreateWalletDialog::onKeyTypeChanged(bool password)
{
   if (!password && !frejaNoticeWasShown_) {
      FrejaNotice dlg(this);

      if (dlg.exec() == QDialog::Accepted) {
         frejaNoticeWasShown_ = true;
      }
   }
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
   bool result = abortWalletCreationQuestionDialog(this);
   if (!result) {
      return;
   }

   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}

bool checkNewWalletValidity(WalletsManager* walletsManager
   , const QString& walletName
   , const std::string& walletId
   , WalletKeysCreateWidget* widgetCreateKeys
   , std::vector<bs::wallet::PasswordData>* keys
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget* parent)
{
   *keys = widgetCreateKeys->keys();

   if (walletsManager->WalletNameExists(walletName.toStdString())) {
      MessageBoxCritical messageBox(QObject::tr("Invalid wallet name")
         , QObject::tr("Wallet with this name already exists"), parent);
      messageBox.exec();
      return false;
   }

   if (!keys->empty() && keys->at(0).encType == bs::wallet::EncryptionType::Freja) {
      if (keys->at(0).encKey.isNull()) {
         MessageBoxCritical messageBox(QObject::tr("Invalid Freja eID")
            , QObject::tr("Please check Freja eID Email"), parent);
         messageBox.exec();
         return false;
      }

      EnterWalletPassword dialog(MobileClientRequest::ActivateWallet, parent);
      dialog.init(walletId, widgetCreateKeys->keyRank(), *keys
         , appSettings, QObject::tr("Activate Freja eID signing"));
      int result = dialog.exec();
      if (!result) {
         return false;
      }

      keys->at(0).password = dialog.GetPassword();

   }
   else if (!widgetCreateKeys->isValid()) {
      MessageBoxCritical messageBox(QObject::tr("Invalid password"), QObject::tr("Please check passwords"), parent);
      messageBox.exec();
      return false;
   }

   WalletPasswordVerifyDialog verifyDialog(appSettings, parent);
   verifyDialog.init(walletId, *keys, widgetCreateKeys->keyRank());
   int result = verifyDialog.exec();
   if (!result) {
      return false;
   }

   return true;
}
