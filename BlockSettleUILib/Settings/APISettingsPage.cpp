/*

***********************************************************************************
* Copyright (C) 2020 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "APISettingsPage.h"
#include "ui_APISettingsPage.h"

#include <spdlog/spdlog.h>
#include <QClipboard>
#include <QPushButton>

#include "ApplicationSettings.h"

APISettingsPage::APISettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::APISettingsPage{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCopyOwnPubKey, &QPushButton::clicked, this, [this] {
      QApplication::clipboard()->setText(ui_->labelOwnPubKey->text());
   });
   connect(ui_->pushButtonApiKeyClear, &QPushButton::clicked, this, [this] {
      ui_->lineEditApiKey->clear();
   });
}

APISettingsPage::~APISettingsPage() = default;

void APISettingsPage::display()
{
   ui_->toggleAutoSign->setChecked(appSettings_->get<bool>(ApplicationSettings::AutoSigning));
   ui_->toggleEnableRFQ->setChecked(appSettings_->get<bool>(ApplicationSettings::AutoQouting));
   ui_->lineEditConnName->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnName));
   ui_->lineEditConnHost->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnHost));
   ui_->lineEditConnPort->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnPort));
   ui_->lineEditConnPubKey->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnPubKey));
   ui_->labelOwnPubKey->setText(appSettings_->get<QString>(ApplicationSettings::ExtConnOwnPubKey));
   ui_->lineEditApiKey->setText(appSettings_->get<QString>(ApplicationSettings::LoginApiKey));
}

void APISettingsPage::reset()
{
   display();
}

void APISettingsPage::apply()
{
   appSettings_->set(ApplicationSettings::AutoSigning, ui_->toggleAutoSign->isChecked());
   appSettings_->set(ApplicationSettings::AutoQouting, ui_->toggleEnableRFQ->isChecked());
   appSettings_->set(ApplicationSettings::ExtConnName, ui_->lineEditConnName->text());
   appSettings_->set(ApplicationSettings::ExtConnHost, ui_->lineEditConnHost->text());
   appSettings_->set(ApplicationSettings::ExtConnPort, ui_->lineEditConnPort->text());
   appSettings_->set(ApplicationSettings::ExtConnPubKey, ui_->lineEditConnPubKey->text());
   appSettings_->set(ApplicationSettings::LoginApiKey, ui_->lineEditApiKey->text());
}
