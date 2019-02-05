#include "ImportWalletDialog.h"
#include "ui_ImportWalletDialog.h"

#include "ApplicationSettings.h"
#include "CreateWalletDialog.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "WalletDeleteDialog.h"
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
      , const std::shared_ptr<spdlog::logger> &logger
      , const QString& username
      , const std::string &walletName, const std::string &walletDesc
      , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::ImportWalletDialog)
   , walletsMgr_(walletsManager)
   , signContainer_(container)
   , appSettings_(appSettings)
   , logger_(logger)
   , armory_(armory)
   , walletSeed_(bs::wallet::Seed::fromEasyCodeChecksum(seedData, chainCodeData
      , appSettings->get<NetworkType>(ApplicationSettings::netType)))
{
   walletId_ = bs::hd::Node(walletSeed_).getId();

   ui_->setupUi(this);

   ui_->lineEditDescription->setValidator(new UiUtils::WalletDescriptionValidator(this));
   
   ui_->labelWalletId->setText(QString::fromStdString(walletId_));

   ui_->checkBoxPrimaryWallet->setEnabled(!walletsManager->HasPrimaryWallet());

   if (!walletsManager->HasPrimaryWallet()) {
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
   ui_->widgetCreateKeys->init(AutheIDClient::ActivateWallet, walletId_, username, appSettings);

   adjustSize();
   setMinimumSize(size());

   if (signContainer_->isOffline() || signContainer_->isWalletOffline(walletId_)) {
      const auto hdWallet = walletsMgr_->GetHDWalletById(walletId_);
      if (hdWallet == nullptr) {
         existingChecked_ = true;
      }
      else {
         BSMessageBox delWoWallet(BSMessageBox::question, tr("WO wallet exists")
            , tr("Watching-only wallet with the same id already exists in the terminal"
               " - do you want to delete it first?"), this);
         if (delWoWallet.exec() == QDialog::Accepted) {
            WalletDeleteDialog delDlg(hdWallet, walletsMgr_, signContainer_, appSettings_, logger, this, true);
            if (delDlg.exec() == QDialog::Accepted) {
               existingChecked_ = true;
            }
         }
      }
   }
   else {
      connect(signContainer_.get(), &SignContainer::HDWalletInfo, this, &ImportWalletDialog::onHDWalletInfo);
      connect(signContainer_.get(), &SignContainer::Error, this, &ImportWalletDialog::onSignerError);
      reqWalletInfoId_ = signContainer_->GetInfo(walletId_);
   }
   updateAcceptButtonState();
}

ImportWalletDialog::~ImportWalletDialog() = default;

void ImportWalletDialog::onError(const QString &errMsg)
{
   BSMessageBox(BSMessageBox::critical, tr("Import wallet error"), errMsg).exec();
   reject();
}

void ImportWalletDialog::onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType>
   , std::vector<SecureBinaryData> &, bs::wallet::KeyRank)
{
   if (id != reqWalletInfoId_) {
      return;
   }
   reqWalletInfoId_ = 0;

   QMetaObject::invokeMethod(this, [this] { promptForSignWalletDelete(); });
}

void ImportWalletDialog::promptForSignWalletDelete()
{
   BSMessageBox delWallet(BSMessageBox::question, tr("Signing wallet exists")
      , tr("Signing wallet with the same id already exists in the terminal"
         " - do you want to delete it first?"), this);
   if (delWallet.exec() == QDialog::Accepted) {
      const auto hdWallet = walletsMgr_->GetHDWalletById(walletId_);
      if (hdWallet == nullptr) {
         BSMessageBox noWoWallet(BSMessageBox::question, tr("Missing WO wallet")
            , tr("Watching-only wallet with id %1 is not loaded in the terminal, but still exists in Signer."
               " Do you want to delete it anyway?").arg(QString::fromStdString(walletId_)), this);
         if (noWoWallet.exec() == QDialog::Accepted) {
            signContainer_->DeleteHDRoot(walletId_);
            existingChecked_ = true;
         }
      } else {
         WalletDeleteDialog delDlg(hdWallet, walletsMgr_, signContainer_, appSettings_, logger_, this, true, true);
         if (delDlg.exec() == QDialog::Accepted) {
            existingChecked_ = true;
         }
      }
      updateAcceptButtonState();
   }
}

void ImportWalletDialog::onSignerError(unsigned int id, std::string error)
{
   if (id != reqWalletInfoId_) {
      return;
   }
   reqWalletInfoId_ = 0;
   existingChecked_ = true;
   updateAcceptButtonState();
}

void ImportWalletDialog::updateAcceptButtonState()
{
   ui_->pushButtonImport->setEnabled(ui_->widgetCreateKeys->isValid()
      && existingChecked_ && !ui_->lineEditWalletName->text().isEmpty());
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
   messageBox.setConfirmButtonText(QObject::tr("Abort"));
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
