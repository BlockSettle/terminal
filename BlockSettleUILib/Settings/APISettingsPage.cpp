/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "APISettingsPage.h"

#include "BSMessageBox.h"
#include "ui_APISettingsPage.h"

#include <spdlog/spdlog.h>
#include <QClipboard>
#include <QInputDialog>
#include <QPushButton>

#include "ApiKeyEntryDialog.h"
#include "ApplicationSettings.h"

APISettingsPage::APISettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::APISettingsPage{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCopyOwnPubKey, &QPushButton::clicked, this, [this] {
      QApplication::clipboard()->setText(ui_->labelOwnPubKey->text());
   });
   connect(ui_->pushButtonApiKeyImport, &QPushButton::clicked, this
      , &APISettingsPage::onApiKeyImport);
   connect(ui_->pushButtonApiKeyClear, &QPushButton::clicked, this, [this] {
      apiKeyEncrypted_.clear();
      updateApiKeyStatus();
   });
}

APISettingsPage::~APISettingsPage() = default;

void APISettingsPage::onApiKeyImport()
{
   auto apiKey = ApiKeyEntryDialog::getApiKey(this);
   if (apiKey.isEmpty()) {
      return;
   }
   auto apiKeyCopy = SecureBinaryData::fromString(apiKey.toStdString());
   if (walletsMgr_ && signContainer_) {
      auto handle = validityFlag_.handle();
      ConfigDialog::encryptData(walletsMgr_, signContainer_, apiKeyCopy
         , [this, handle](ConfigDialog::EncryptError error, const SecureBinaryData& data) {
         if (!handle.isValid()) {
            return;
         }
         if (error != ConfigDialog::EncryptError::NoError) {
            BSMessageBox(BSMessageBox::critical, tr("Import")
               , tr("API key import failed")
               , ConfigDialog::encryptErrorStr(error)
               , this).exec();
            return;
         }
         apiKeyEncrypted_ = data.toHexStr();
         updateApiKeyStatus();
      });
   }
   else {
      //TODO: implement API key import without walletsMgr_ and signContainer_
   }
}

void APISettingsPage::display()
{
   if (appSettings_) {
      ui_->toggleAutoSign->setChecked(appSettings_->get<bool>(ApplicationSettings::AutoSigning));
      ui_->toggleEnableRFQ->setChecked(appSettings_->get<bool>(ApplicationSettings::AutoQouting));
      ui_->lineEditConnName->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnName));
      ui_->lineEditConnHost->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnHost));
      ui_->lineEditConnPort->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnPort));
      ui_->lineEditConnPubKey->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnPubKey));
      ui_->labelOwnPubKey->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnOwnPubKey));

      apiKeyEncrypted_ = appSettings_->get<std::string>(ApplicationSettings::LoginApiKey);
   }
   else {
      ui_->toggleAutoSign->setChecked(settings_.at(ApplicationSettings::AutoSigning).toBool());
      ui_->toggleEnableRFQ->setChecked(settings_.at(ApplicationSettings::AutoQouting).toBool());
      ui_->lineEditConnName->setEnabled(false);
      ui_->lineEditConnHost->setEnabled(false);
      ui_->lineEditConnPort->setText(settings_.at(ApplicationSettings::ExtConnPort).toString());
      ui_->lineEditConnPubKey->setEnabled(false);
      ui_->labelOwnPubKey->setEnabled(false);

      apiKeyEncrypted_ = settings_.at(ApplicationSettings::LoginApiKey).toString().toStdString();
   }
   updateApiKeyStatus();
}

void APISettingsPage::reset()
{
   display();
}

void APISettingsPage::apply()
{
   if (appSettings_) {
      appSettings_->set(ApplicationSettings::AutoSigning, ui_->toggleAutoSign->isChecked());
      appSettings_->set(ApplicationSettings::AutoQouting, ui_->toggleEnableRFQ->isChecked());
      appSettings_->set(ApplicationSettings::ExtConnName, ui_->lineEditConnName->text());
      appSettings_->set(ApplicationSettings::ExtConnHost, ui_->lineEditConnHost->text());
      appSettings_->set(ApplicationSettings::ExtConnPort, ui_->lineEditConnPort->text());
      appSettings_->set(ApplicationSettings::ExtConnPubKey, ui_->lineEditConnPubKey->text());
      appSettings_->set(ApplicationSettings::LoginApiKey, QString::fromStdString(apiKeyEncrypted_));
   }
   else {
      emit putSetting(ApplicationSettings::AutoSigning, ui_->toggleAutoSign->isChecked());
      emit putSetting(ApplicationSettings::AutoQouting, ui_->toggleEnableRFQ->isChecked());
      emit putSetting(ApplicationSettings::ExtConnPort, ui_->lineEditConnPort->text());
      emit putSetting(ApplicationSettings::LoginApiKey, QString::fromStdString(apiKeyEncrypted_));
   }
}

void APISettingsPage::updateApiKeyStatus()
{
   if (apiKeyEncrypted_.empty()) {
      ui_->labelApiKeyStatus->clear();
   } else {
      ui_->labelApiKeyStatus->setText(tr("Imported"));
   }
}
