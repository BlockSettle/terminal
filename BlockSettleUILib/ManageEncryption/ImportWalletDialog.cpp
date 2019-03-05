#include "ImportWalletDialog.h"
#include "ui_ImportWalletDialog.h"

#include "ApplicationSettings.h"
#include "CreateWalletDialog.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "WalletDeleteDialog.h"
#include "WalletImporter.h"
#include "WalletPasswordVerifyDialog.h"
#include "WalletKeysCreateWidget.h"
#include "EnterWalletPassword.h"
#include "WalletsManager.h"
#include "UiUtils.h"
#include "QWalletInfo.h"

#include <spdlog/spdlog.h>


ImportWalletDialog::ImportWalletDialog(const std::shared_ptr<WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<AssetManager> &assetMgr
      , const std::shared_ptr<AuthAddressManager> &authMgr, const std::shared_ptr<ArmoryConnection> &armory
      , const EasyCoDec::Data& seedData
      , const EasyCoDec::Data& chainCodeData
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<spdlog::logger> &logger
      , const QString& username
      , const std::string &walletName, const std::string &walletDesc
      , bool disableImportPrimary
      , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::ImportWalletDialog)
   , walletsManager_(walletsManager)
   , signContainer_(container)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
   , logger_(logger)
   , armory_(armory)
   , walletSeed_(bs::wallet::Seed::fromEasyCodeChecksum(seedData, chainCodeData
   , appSettings->get<NetworkType>(ApplicationSettings::netType)))
   , disableImportPrimary_{disableImportPrimary}
{
   walletInfo_.setRootId(bs::hd::Node(walletSeed_).getId());

   ui_->setupUi(this);

   ui_->lineEditDescription->setValidator(new UiUtils::WalletDescriptionValidator(this));

   ui_->labelWalletId->setText(walletInfo_.rootId());

   if (!walletsManager->HasPrimaryWallet() && !disableImportPrimary_) {
      setWindowTitle(tr("Import Primary Wallet"));
      ui_->checkBoxPrimaryWallet->setChecked(true);
      ui_->checkBoxPrimaryWallet->setEnabled(true);
      ui_->lineEditWalletName->setText(tr("Primary wallet"));
   } else {
      setWindowTitle(tr("Import Wallet"));
      ui_->checkBoxPrimaryWallet->setEnabled(false);
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

   connect(ui_->lineEditWalletName, &QLineEdit::returnPressed, this, &ImportWalletDialog::importWallet);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &ImportWalletDialog::importWallet);


   connect(ui_->lineEditWalletName, &QLineEdit::textChanged,
      this, &ImportWalletDialog::updateAcceptButtonState);
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged,
      [this] { updateAcceptButtonState(); });

   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });

//   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideWidgetContol
//      | WalletKeysCreateWidget::HideAuthConnectButton);
   //ui_->widgetCreateKeys->init(AutheIDClient::ActivateWallet, walletId_, username, appSettings);

   ui_->widgetCreateKeys->init(AutheIDClient::ActivateWallet
      , walletInfo_, WalletKeyWidget::UseType::ChangeAuthForDialog, appSettings, connectionManager, logger);

   adjustSize();
   setMinimumSize(size());

   if (signContainer_->isOffline() || signContainer_->isWalletOffline(walletInfo_.rootId().toStdString())) {
      const auto hdWallet = walletsManager_->GetHDWalletById(walletInfo_.rootId().toStdString());
      if (hdWallet == nullptr) {
         existingChecked_ = true;
      }
      else {
         BSMessageBox delWoWallet(BSMessageBox::question, tr("WO wallet exists")
            , tr("Watching-only wallet with the same id already exists in the terminal"
               " - do you want to delete it first?"), this);
         if (delWoWallet.exec() == QDialog::Accepted) {
            WalletDeleteDialog delDlg(hdWallet, walletsManager_, signContainer_, appSettings_, connectionManager_, logger, this, true);
            if (delDlg.exec() == QDialog::Accepted) {
               existingChecked_ = true;
            }
         }
      }
   }
   else {
      connect(signContainer_.get(), &SignContainer::QWalletInfo, this, &ImportWalletDialog::onWalletInfo);
      connect(signContainer_.get(), &SignContainer::Error, this, &ImportWalletDialog::onSignerError);
      reqWalletInfoId_ = signContainer_->GetInfo(walletInfo_.rootId().toStdString());
   }
   updateAcceptButtonState();
}

ImportWalletDialog::~ImportWalletDialog() = default;

void ImportWalletDialog::onError(const QString &errMsg)
{
   BSMessageBox(BSMessageBox::critical, tr("Import wallet error"), errMsg).exec();
   reject();
}

void ImportWalletDialog::onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo &walletInfo)
{
   if (reqId != reqWalletInfoId_) {
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
      const auto hdWallet = walletsManager_->GetHDWalletById(walletInfo_.rootId().toStdString());
      if (hdWallet == nullptr) {
         BSMessageBox noWoWallet(BSMessageBox::question, tr("Missing WO wallet")
            , tr("Watching-only wallet with id %1 is not loaded in the terminal, but still exists in Signer."
               " Do you want to delete it anyway?").arg(walletInfo_.rootId()), this);
         if (noWoWallet.exec() == QDialog::Accepted) {
            signContainer_->DeleteHDRoot(walletInfo_.rootId().toStdString());
            existingChecked_ = true;
         }
      } else {
         WalletDeleteDialog delDlg(hdWallet, walletsManager_, signContainer_, appSettings_, connectionManager_, logger_, this, true, true);
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
   ui_->pushButtonImport->setEnabled(ui_->widgetCreateKeys->isValid() &&
      !ui_->lineEditWalletName->text().isEmpty());
   ui_->pushButtonImport->setEnabled(ui_->widgetCreateKeys->isValid()
      && existingChecked_ && !ui_->lineEditWalletName->text().isEmpty());
}

void ImportWalletDialog::onWalletCreated(const std::string &walletId)
{
   if (armory_->state() == ArmoryConnection::State::Ready) {
      emit walletsManager_->walletImportStarted(walletId);
   }
   else {
      const auto &rootWallet = walletsManager_->GetHDWalletById(walletId);
      if (rootWallet) {
         for (const auto &leaf : rootWallet->getLeaves()) {
            appSettings_->SetWalletScanIndex(leaf->GetWalletId(), 0);
         }
      }
   }
   accept();
}

void ImportWalletDialog::importWallet()
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

      dialog.init(walletInfo_, appSettings_, connectionManager_, WalletKeyWidget::UseType::ChangeToEidAsDialog
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


   try {
      importedAsPrimary_ = ui_->checkBoxPrimaryWallet->isChecked();

      ui_->pushButtonImport->setEnabled(false);

      std::vector<bs::wallet::PasswordData> vectorPwData;
      vectorPwData.assign(pwData.cbegin(), pwData.cend());

      walletImporter_->Import(walletInfo_.name().toStdString(), walletInfo_.desc().toStdString(), walletSeed_
         , importedAsPrimary_, vectorPwData, ui_->widgetCreateKeys->keyRank());
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
