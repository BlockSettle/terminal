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
#include <QFileDialog>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "AutoSignQuoteProvider.h"
#include "UserScript.h"
#include "ApplicationSettings.h"


APISettingsPage::APISettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::APISettingsPage{}}
{
   ui_->setupUi(this);
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
}
