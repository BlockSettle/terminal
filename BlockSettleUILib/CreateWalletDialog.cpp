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

#include <QValidator>

#include <spdlog/spdlog.h>


//
// DescriptionValidator
//

//! Validator for description of wallet.
class DescriptionValidator final : public QValidator
{
public:
   explicit DescriptionValidator(QObject *parent)
      : QValidator(parent)
   {
   }


   QValidator::State validate(QString &input, int &pos) const override
   {
      static const QString invalidCharacters = QLatin1String("\\/?:*<>|");

      if (input.isEmpty()) {
         return QValidator::Acceptable;
      }

      if (invalidCharacters.contains(input.at(pos - 1))) {
         input.remove(pos - 1, 1);

         if (pos > input.size()) {
            --pos;
         }

         return QValidator::Invalid;
      } else {
         return QValidator::Acceptable;
      }
   }
};


CreateWalletDialog::CreateWalletDialog(const std::shared_ptr<WalletsManager>& walletsManager
   , const std::shared_ptr<SignContainer> &container, const QString &walletsPath
   , const bs::wallet::Seed& walletSeed, const std::string& walletId, bool createPrimary, const QString& username
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::CreateWalletDialog)
   , walletsManager_(walletsManager)
   , signingContainer_(container)
   , walletSeed_(walletSeed)
   , walletId_(walletId)
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

   ui_->lineEditDescription->setValidator(new DescriptionValidator(this));

   ui_->labelWalletId->setText(QString::fromStdString(walletId_));

   //connect(ui_->lineEditWalletName, &QLineEdit::textChanged, this, &CreateWalletDialog::UpdateAcceptButtonState);
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { UpdateAcceptButtonState(); });

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideWidgetContol | WalletKeysCreateWidget::HideFrejaConnectButton);
   ui_->widgetCreateKeys->init(walletId_, username);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);
   connect(ui_->lineEditDescription, &QLineEdit::returnPressed, this, &CreateWalletDialog::CreateWallet);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &CreateWalletDialog::CreateWallet);
   //connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateWalletDialog::reject);

   connect(signingContainer_.get(), &SignContainer::HDWalletCreated, this, &CreateWalletDialog::onWalletCreated);
   connect(signingContainer_.get(), &SignContainer::Error, this, &CreateWalletDialog::onWalletCreateError);

   //UpdateAcceptButtonState();
}

CreateWalletDialog::~CreateWalletDialog() = default;

//void CreateWalletDialog::showEvent(QShowEvent *event)
//{
//   ui_->labelHintPrimary->setVisible(ui_->checkBoxPrimaryWallet->isVisible());
//   QDialog::showEvent(event);
//}

void CreateWalletDialog::CreateWallet()
{
   if (walletsManager_->WalletNameExists(ui_->lineEditWalletName->text().toStdString())) {
      MessageBoxCritical messageBox(tr("Invalid wallet name"), tr("Wallet with this name already exists"), this);
      messageBox.exec();
      return;
   }

   std::vector<bs::wallet::PasswordData> keys = ui_->widgetCreateKeys->keys();

   if (!keys.empty() && keys.at(0).encType == bs::wallet::EncryptionType::Freja) {
      if (keys.at(0).encKey.isNull()) {
         MessageBoxCritical messageBox(tr("Invalid Freja eID"), tr("Please check Freja eID Email"), this);
         messageBox.exec();
         return;
      }

      std::vector<bs::wallet::EncryptionType> encTypes;
      std::vector<SecureBinaryData> encKeys;
      for (const bs::wallet::PasswordData& key : keys) {
         encTypes.push_back(key.encType);
         encKeys.push_back(key.encKey);
      }

      EnterWalletPassword dialog(walletId_, ui_->widgetCreateKeys->keyRank(), encTypes, encKeys
         , tr("Activate Freja eID signing"), this);
      int result = dialog.exec();
      if (!result) {
         return;
      }

      keys.at(0).password = dialog.GetPassword();

   } else if (!ui_->widgetCreateKeys->isValid()) {
      MessageBoxCritical messageBox(tr("Invalid password"), tr("Please check passwords"), this);
      messageBox.exec();
      return;
   }

   WalletPasswordVerifyDialog verifyDialog(walletId_, keys, ui_->widgetCreateKeys->keyRank(), this);
   int result = verifyDialog.exec();
   if (!result) {
      return;
   }

   ui_->pushButtonContinue->setEnabled(false);

   const auto &name = ui_->lineEditWalletName->text().toStdString();
   const auto &description = ui_->lineEditDescription->text().toStdString();
   createReqId_ = signingContainer_->CreateHDWallet(name, description
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
