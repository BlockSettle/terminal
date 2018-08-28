#include "ImportWalletDialog.h"
#include "ui_ImportWalletDialog.h"

#include "ApplicationSettings.h"
#include "EnterWalletPassword.h"
#include "MessageBoxCritical.h"
#include "SignContainer.h"
#include "WalletImporter.h"
#include "WalletPasswordVerifyDialog.h"
#include "WalletsManager.h"

#include <spdlog/spdlog.h>


ImportWalletDialog::ImportWalletDialog(const std::shared_ptr<WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<AssetManager> &assetMgr
      , const std::shared_ptr<AuthAddressManager> &authMgr, const std::shared_ptr<ArmoryConnection> &armory
      , const EasyCoDec::Data& seedData
      , const EasyCoDec::Data& chainCodeData
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const QString& username
      , const std::string &walletName, const std::string &walletDesc
      , bool createPrimary, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::ImportWalletDialog)
   , walletsMgr_(walletsManager)
   , appSettings_(appSettings)
   , armory_(armory)
   , walletSeed_(bs::wallet::Seed::fromEasyCodeChecksum(seedData, chainCodeData
      , appSettings->get<NetworkType>(ApplicationSettings::netType)))
{
   walletId_ = bs::hd::Node(walletSeed_).getId();

   ui_->setupUi(this);
   
   ui_->labelWalletId->setText(QString::fromStdString(walletId_));

   ui_->checkBoxPrimaryWallet->setEnabled(!walletsManager->HasPrimaryWallet());

   if (createPrimary && !walletsManager->HasPrimaryWallet()) {
      setWindowTitle(tr("Import Primary Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(true);
      ui_->lineEditWalletName->setText(tr("Primary wallet"));
   } else {
      setWindowTitle(tr("Import Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(false);
      ui_->lineEditWalletName->setText(tr("Wallet #%1").arg(walletsManager->GetWalletsCount() + 1));
   }

   if (!walletName.empty()) {
      ui_->lineEditWalletName->setText(QString::fromStdString(walletName));
   }
   ui_->lineEditDescription->setText(QString::fromStdString(walletDesc));

   const auto &cbr = [] (const std::string &walletId) -> unsigned int {
      return 0;
   };
   const auto &cbw = [appSettings] (const std::string &walletId, unsigned int idx) {
      appSettings->SetWalletScanIndex(walletId, idx);
   };
   walletImporter_ = std::make_shared<WalletImporter>(container, walletsManager, armory
      , assetMgr, authMgr, appSettings_->GetHomeDir(), cbr, cbw);

   connect(walletImporter_.get(), &WalletImporter::walletCreated, this, &ImportWalletDialog::onWalletCreated);
   connect(walletImporter_.get(), &WalletImporter::error, this, &ImportWalletDialog::onError);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &ImportWalletDialog::onImportAccepted);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &ImportWalletDialog::onImportAccepted);

   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideWidgetContol 
      | WalletKeysCreateWidget::HideFrejaConnectButton);
   ui_->widgetCreateKeys->init(walletId_, username);

   adjustSize();
   setMinimumSize(size());
}

ImportWalletDialog::~ImportWalletDialog() = default;

void ImportWalletDialog::onError(const QString &errMsg)
{
   MessageBoxCritical(tr("Import wallet error"), errMsg).exec();
   reject();
}

void ImportWalletDialog::onWalletCreated(const std::string &walletId)
{
   if (armory_->state() == ArmoryConnection::State::Ready) {
      emit walletsMgr_->walletImportStarted(walletId);
   }
   else {
      const auto &rootWallet = walletsMgr_->GetHDWalletById(walletId);
      if (rootWallet) {
         for (const auto &leaf : rootWallet->getLeaves()) {
            appSettings_->SetWalletScanIndex(leaf->GetWalletId(), 0);
         }
      }
   }
   accept();
}

void ImportWalletDialog::onImportAccepted()
{
   if (walletsMgr_->WalletNameExists(ui_->lineEditWalletName->text().toStdString())) {
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

   }
   else if (!ui_->widgetCreateKeys->isValid()) {
      MessageBoxCritical messageBox(tr("Invalid password"), tr("Please check passwords"), this);
      messageBox.exec();
      return;
   }

   WalletPasswordVerifyDialog verifyDialog(walletId_, keys, ui_->widgetCreateKeys->keyRank(), this);
   int result = verifyDialog.exec();
   if (!result) {
      return;
   }

   try {
      walletName_ = ui_->lineEditWalletName->text();
      auto description = ui_->lineEditDescription->text().toStdString();
      importedAsPrimary_ = ui_->checkBoxPrimaryWallet->isChecked();

      ui_->pushButtonImport->setEnabled(false);

      walletImporter_->Import(walletName_.toStdString(), description, walletSeed_
         , importedAsPrimary_, keys, ui_->widgetCreateKeys->keyRank());
   }
   catch (...) {
      onError(tr("Invalid backup data"));
   }
}

void ImportWalletDialog::reject()
{
   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}
