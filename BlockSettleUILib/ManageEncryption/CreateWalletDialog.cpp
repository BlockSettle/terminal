#include "CreateWalletDialog.h"
#include "ui_CreateWalletDialog.h"

#include "HDWallet.h"
#include "BSMessageBox.h"
#include "WalletPasswordVerifyDialog.h"
#include "NewWalletSeedDialog.h"
#include "SignContainer.h"
#include "EnterWalletPassword.h"
#include "WalletsManager.h"
#include "WalletKeysCreateWidget.h"
#include "UiUtils.h"

#include <spdlog/spdlog.h>


CreateWalletDialog::CreateWalletDialog(const std::shared_ptr<WalletsManager>& walletsManager
   , const std::shared_ptr<SignContainer> &container
   , const QString &walletsPath
   , const bs::wallet::Seed& walletSeed
   , const std::string& walletId
   , const QString& username
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<spdlog::logger> &logger
   , QWidget *parent)

   : QDialog(parent)
   , ui_(new Ui::CreateWalletDialog)
   , walletsManager_(walletsManager)
   , signingContainer_(container)
   , appSettings_(appSettings)
   , walletsPath_(walletsPath)
   , walletSeed_(walletSeed)
   , logger_(logger)
{
   ui_->setupUi(this);
   walletInfo_.setRootId(walletId);

   ui_->checkBoxPrimaryWallet->setEnabled(!walletsManager->HasPrimaryWallet());
   ui_->checkBoxPrimaryWallet->setChecked(!walletsManager->HasPrimaryWallet());

   if (!walletsManager->HasPrimaryWallet()) {
      setWindowTitle(tr("Create Primary Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(true);

      ui_->lineEditWalletName->setText(tr("Primary Wallet"));
   } else {
      setWindowTitle(tr("Create New Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(false);

      ui_->lineEditWalletName->setText(tr("Wallet #%1").arg(walletsManager->GetWalletsCount() + 1));
   }

   ui_->lineEditDescription->setValidator(new UiUtils::WalletDescriptionValidator(this));

   ui_->labelWalletId->setText(QString::fromStdString(walletId));

   connect(ui_->lineEditWalletName, &QLineEdit::textChanged, this, &CreateWalletDialog::updateAcceptButtonState);
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateAcceptButtonState(); });

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideWidgetContol | WalletKeysCreateWidget::HideAuthConnectButton);

   // for eid wallet signing suggest email used for login into app

   ui_->widgetCreateKeys->init(AutheIDClient::ActivateWallet
      , walletInfo_, WalletKeyWidget::UseType::ChangeAuthForDialog, appSettings, logger);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &CreateWalletDialog::createWallet);
   connect(ui_->lineEditDescription, &QLineEdit::returnPressed, this, &CreateWalletDialog::createWallet);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &CreateWalletDialog::createWallet);
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

void CreateWalletDialog::createWallet()
{
   // currently {1,1} key created on wallet creation
   walletInfo_.setName(ui_->lineEditWalletName->text());
   walletInfo_.setDesc(ui_->lineEditDescription->text());
   walletInfo_.setPasswordData(ui_->widgetCreateKeys->passwordData());
   walletInfo_.setKeyRank({1,1});


   // check wallet name
   if (walletsManager_->WalletNameExists(walletInfo_.name().toStdString())) {
      BSMessageBox messageBox(BSMessageBox::critical, QObject::tr("Invalid wallet name")
         , QObject::tr("Wallet with this name already exists"), this);
      messageBox.exec();
      return;
   }

   std::vector<bs::wallet::PasswordData> pwData = ui_->widgetCreateKeys->passwordData();

   // request eid auth if it's selected
   if (ui_->widgetCreateKeys->passwordData()[0].encType == bs::wallet::EncryptionType::Auth) {
      if (ui_->widgetCreateKeys->passwordData()[0].encKey.isNull()) {
         BSMessageBox messageBox(BSMessageBox::critical, QObject::tr("Invalid Auth eID")
            , QObject::tr("Please check Auth eID Email"), this);
         messageBox.exec();
         return;
      }

      EnterWalletPassword dialog(AutheIDClient::ActivateWallet, this);

      dialog.init(walletInfo_, appSettings_, WalletKeyWidget::UseType::ChangeToEidAsDialog
         , QObject::tr("Activate Auth eID Signing"), logger_, QObject::tr("Auth eID"));
      int result = dialog.exec();
      if (!result) {
         return;
      }

      walletInfo_.setPasswordData(dialog.passwordData());
      pwData = dialog.passwordData();
   }
   else if (!ui_->widgetCreateKeys->isValid()) {
      BSMessageBox messageBox(BSMessageBox::critical, QObject::tr("Invalid password")
         , QObject::tr("Please check the password"), this);
      messageBox.exec();
   }

   WalletPasswordVerifyDialog verifyDialog(appSettings_, this);
   verifyDialog.init(walletInfo_, pwData, logger_);
   int result = verifyDialog.exec();
   if (!result) {
      return;
   }

   ui_->pushButtonContinue->setEnabled(false);

   std::vector<bs::wallet::PasswordData> vectorPwData;
   vectorPwData.assign(pwData.cbegin(), pwData.cend());

   createReqId_ = signingContainer_->CreateHDWallet(walletInfo_.name().toStdString()
      , walletInfo_.desc().toStdString()
      , ui_->checkBoxPrimaryWallet->isChecked()
      , walletSeed_, vectorPwData
      , ui_->widgetCreateKeys->keyRank());
}

void CreateWalletDialog::onWalletCreateError(unsigned int id, std::string errMsg)
{
   if (!createReqId_ || (createReqId_ != id)) {
      return;
   }
   createReqId_ = 0;
   BSMessageBox info(BSMessageBox::critical, tr("Create failed")
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
   if (walletInfo_.rootId().toStdString() != wallet->getWalletId()) {
      BSMessageBox(BSMessageBox::critical, tr("Wallet ID mismatch")
         , tr("Pre-created wallet id: %1, id after creation: %2")
            .arg(walletInfo_.rootId()).arg(QString::fromStdString(wallet->getWalletId()))
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
   bool result = MessageBoxWalletCreateAbort(this).exec();
   if (!result) {
      return;
   }

   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}
