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
   , frejaSign_(spdlog::get(""))
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
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &ImportWalletDialog::updateAcceptButton);
   connect(ui_->lineEditPasswordConfirm, &QLineEdit::textChanged, this, &ImportWalletDialog::updateAcceptButton);

   connect(ui_->radioButtonPassword, &QRadioButton::clicked, this, &ImportWalletDialog::onEncTypeChanged);
   connect(ui_->radioButtonFreja, &QRadioButton::clicked, this, &ImportWalletDialog::onEncTypeChanged);
   connect(ui_->lineEditFrejaId, &QLineEdit::textChanged, this, &ImportWalletDialog::onFrejaIdChanged);
   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &ImportWalletDialog::startFrejaSign);

   connect(&frejaSign_, &FrejaSignWallet::succeeded, this, &ImportWalletDialog::onFrejaSucceeded);
   connect(&frejaSign_, &FrejaSign::failed, this, &ImportWalletDialog::onFrejaFailed);
   connect(&frejaSign_, &FrejaSign::statusUpdated, this, &ImportWalletDialog::onFrejaStatusUpdated);

   updateAcceptButton();
   onEncTypeChanged();
}

bool ImportWalletDialog::couldImport() const
{
   return (!ui_->lineEditWalletName->text().isEmpty()
         && !walletPassword_.isNull());
}

void ImportWalletDialog::updateAcceptButton()
{
   ui_->pushButtonImport->setEnabled(couldImport());
}

void ImportWalletDialog::onPasswordChanged(const QString &)
{
   if (!ui_->lineEditPassword->text().isEmpty()
      && (ui_->lineEditPassword->text() == ui_->lineEditPasswordConfirm->text())) {
      walletPassword_ = ui_->lineEditPassword->text().toStdString();
   }
   updateAcceptButton();
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

      if (ui_->radioButtonFreja->isChecked()) {
         walletSeed_.setEncryptionKey(ui_->lineEditFrejaId->text().toStdString());
      }

      ui_->pushButtonImport->setEnabled(false);

      walletImporter_->Import(walletName_.toStdString(), description, walletSeed_
         , importedAsPrimary_, walletPassword_);
   }
   catch (...) {
      onError(tr("Invalid backup data"));
   }
}

void ImportWalletDialog::onFrejaIdChanged(const QString &)
{
   ui_->pushButtonFreja->setEnabled(!ui_->lineEditFrejaId->text().isEmpty());
}

void ImportWalletDialog::onEncTypeChanged()
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

void ImportWalletDialog::startFrejaSign()
{
   frejaSign_.start(ui_->lineEditFrejaId->text(), tr("Wallet %1 importing")
      .arg(ui_->lineEditWalletName->text()), walletId_);
   ui_->pushButtonFreja->setEnabled(false);
   ui_->lineEditFrejaId->setEnabled(false);
}

void ImportWalletDialog::onFrejaSucceeded(SecureBinaryData password)
{
   ui_->labelFreja->setText(tr("Successfully signed"));
   walletPassword_ = password;
   updateAcceptButton();
}

void ImportWalletDialog::onFrejaFailed(const QString &text)
{
   ui_->pushButtonFreja->setEnabled(true);
   ui_->labelFreja->setText(tr("Freja failed: %1").arg(text));
}

void ImportWalletDialog::onFrejaStatusUpdated(const QString &status)
{
   ui_->labelFreja->setText(status);
}
