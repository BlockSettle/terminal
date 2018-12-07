#include "ImportWalletDialog.h"
#include "ui_ImportWalletDialog.h"

#include "ApplicationSettings.h"
#include "CreateWalletDialog.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "WalletImporter.h"
#include "WalletPasswordVerifyDialog.h"
#include "WalletsManager.h"
#include "UiUtils.h"

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

   ui_->lineEditDescription->setValidator(new UiUtils::WalletDescriptionValidator(this));
   
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

   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyTypeChanged,
      this, &ImportWalletDialog::onKeyTypeChanged);
   connect(ui_->lineEditWalletName, &QLineEdit::textChanged,
      this, &ImportWalletDialog::updateAcceptButtonState);
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged,
      [this] { updateAcceptButtonState(); });

   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideWidgetContol 
      | WalletKeysCreateWidget::HideAuthConnectButton);
   ui_->widgetCreateKeys->init(MobileClient::ActivateWallet, walletId_, username, appSettings);

   adjustSize();
   setMinimumSize(size());
}

ImportWalletDialog::~ImportWalletDialog() = default;

void ImportWalletDialog::onError(const QString &errMsg)
{
   BSMessageBox(BSMessageBox::critical, tr("Import wallet error"), errMsg).exec();
   reject();
}

void ImportWalletDialog::updateAcceptButtonState()
{
   ui_->pushButtonImport->setEnabled(ui_->widgetCreateKeys->isValid() &&
      !ui_->lineEditWalletName->text().isEmpty());
}

void ImportWalletDialog::onKeyTypeChanged(bool password)
{
   if (!password && !authNoticeWasShown_) {
      MessageBoxAuthNotice dlg(this);

      if (dlg.exec() == QDialog::Accepted) {
         authNoticeWasShown_ = true;
      }
   }
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
   walletName_ = ui_->lineEditWalletName->text();
   const QString &walletDescription = ui_->lineEditDescription->text();
   std::vector<bs::wallet::PasswordData> keys;

   bool result = checkNewWalletValidity(walletsMgr_.get(), walletName_, walletId_
      , ui_->widgetCreateKeys, &keys, appSettings_, this);
   if (!result) {
      return;
   }

   try {
      importedAsPrimary_ = ui_->checkBoxPrimaryWallet->isChecked();

      ui_->pushButtonImport->setEnabled(false);

      walletImporter_->Import(walletName_.toStdString(), walletDescription.toStdString(), walletSeed_
         , importedAsPrimary_, keys, ui_->widgetCreateKeys->keyRank());
   }
   catch (...) {
      onError(tr("Invalid backup data"));
   }
}

bool abortWalletImportQuestionDialog(QWidget* parent)
{
   BSMessageBox messageBox(BSMessageBox::question, QObject::tr("Warning"), QObject::tr("Do you want to abort Wallet Import?")
      , QObject::tr("The Wallet will not be imported if you don't complete the procedure.\n\n"
         "Are you sure you want to abort the Wallet Import process?"), parent);
   messageBox.setConfirmButtonText(QObject::tr("Abort\nWallet Import"));
   messageBox.setCancelButtonText(QObject::tr("Back"));

   int result = messageBox.exec();
   return (result == QDialog::Accepted);
}

void ImportWalletDialog::reject()
{
   bool result = abortWalletImportQuestionDialog(this);
   if (!result) {
      return;
   }

   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}
