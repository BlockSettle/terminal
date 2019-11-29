/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "NewWalletDialog.h"
#include "ui_NewWalletDialog.h"

#include "ApplicationSettings.h"

NewWalletDialog::NewWalletDialog(bool noWalletsFound, const std::shared_ptr<ApplicationSettings>& appSettings, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::NewWalletDialog)
{
   ui_->setupUi(this);

   if (noWalletsFound) {
      ui_->labelPurpose->setText(tr("THE TERMINAL CAN'T FIND ANY EXISTING WALLETS"));
   }

   const auto messageText = tr("<html><head/><body><p>For guidance, please consult the <a href=\"%1\"><span style=\" padding-left: 5px; text-decoration: none; color:#ffffff;\">\"Getting Started Guide\".</span></a></p></body></html>").arg(appSettings->get<QString>(ApplicationSettings::GettingStartedGuide_Url));

   ui_->labelMessage->setText(messageText);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, [this] {
      isCreate_ = true;
      accept();
   });
   connect(ui_->pushButtonImport, &QPushButton::clicked, [this] {
      isImport_ = true;
      accept();
   });
}

NewWalletDialog::~NewWalletDialog() = default;
