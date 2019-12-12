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
#include "BSMessageBox.h"

NewWalletDialog::NewWalletDialog(bool noWalletsFound, const std::shared_ptr<ApplicationSettings>& appSettings, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::NewWalletDialog)
{
   ui_->setupUi(this);

   if (noWalletsFound) {
      ui_->labelPurpose->setText(tr("THE TERMINAL CAN'T FIND ANY EXISTING WALLETS"));
   }

   // Use ApplicationSettings::GettingStartedGuide_Url carefully with arg() - it contains '%' symbol
   const auto messageText =
         tr("For guidance, please consult the ")
         + QStringLiteral("<a href=\"") + appSettings->get<QString>(ApplicationSettings::GettingStartedGuide_Url) + QStringLiteral("\">")
         + QStringLiteral("<span style=\"text-decoration: underline; color: %1;\">Getting Started Guide.</span></a>")
         .arg(BSMessageBox::kUrlColor);

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
