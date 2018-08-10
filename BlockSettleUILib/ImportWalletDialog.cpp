#include "ImportWalletDialog.h"
#include "ui_ImportWalletDialog.h"

#include "ApplicationSettings.h"
#include "MessageBoxCritical.h"
#include "SignContainer.h"
#include "WalletImporter.h"
#include "WalletsManager.h"

#include <spdlog/spdlog.h>


ImportWalletDialog::ImportWalletDialog(const std::shared_ptr<WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<AssetManager> &assetMgr
      , const std::shared_ptr<AuthAddressManager> &authMgr
      , const EasyCoDec::Data& seedData
      , const EasyCoDec::Data& chainCodeData
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::string &walletName, const std::string &walletDesc
      , bool createPrimary, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::ImportWalletDialog)
   , walletsMgr_(walletsManager)
   , appSettings_(appSettings)
   , walletSeed_(bs::wallet::Seed::fromEasyCodeChecksum(seedData, chainCodeData
      , appSettings->get<NetworkType>(ApplicationSettings::netType)))
{
   walletId_ = bs::hd::Node(walletSeed_).getId();

   ui_->setupUi(this);

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
   walletImporter_ = std::make_shared<WalletImporter>(container, walletsManager, PyBlockDataManager::instance()
      , assetMgr, authMgr, appSettings_->GetHomeDir(), cbr, cbw);

   connect(walletImporter_.get(), &WalletImporter::walletCreated, this, &ImportWalletDialog::onWalletCreated);
   connect(walletImporter_.get(), &WalletImporter::error, this, &ImportWalletDialog::onError);

   connect(ui_->lineEditWalletName, &QLineEdit::textChanged, this, &ImportWalletDialog::updateAcceptButton);

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &ImportWalletDialog::onImportAccepted);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &ImportWalletDialog::onImportAccepted);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ImportWalletDialog::reject);

   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateAcceptButton(); });
   ui_->widgetCreateKeys->init(walletId_);

   updateAcceptButton();
}

bool ImportWalletDialog::couldImport() const
{
   return (!ui_->lineEditWalletName->text().isEmpty()
         && ui_->widgetCreateKeys->isValid());
}

void ImportWalletDialog::updateAcceptButton()
{
   ui_->pushButtonImport->setEnabled(couldImport());
}

void ImportWalletDialog::onError(const QString &errMsg)
{
   MessageBoxCritical(tr("Import wallet error"), errMsg).exec();
   reject();
}

void ImportWalletDialog::onWalletCreated(const std::string &walletId)
{
   if (PyBlockDataManager::instance()->GetState() == PyBlockDataManagerState::Ready) {
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
   if (!couldImport()) {
      return;
   }

   try {
      walletName_ = ui_->lineEditWalletName->text();
      auto description = ui_->lineEditDescription->text().toStdString();
      importedAsPrimary_ = ui_->checkBoxPrimaryWallet->isChecked();

      ui_->pushButtonImport->setEnabled(false);

      walletImporter_->Import(walletName_.toStdString(), description, walletSeed_
         , importedAsPrimary_, ui_->widgetCreateKeys->keys(), ui_->widgetCreateKeys->keyRank());
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
