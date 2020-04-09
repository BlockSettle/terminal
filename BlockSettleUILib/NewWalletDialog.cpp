/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "NewWalletDialog.h"
#include "ui_NewWalletDialog.h"

#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "InfoDialogs/SupportDialog.h"

namespace {
   const QString kSupportDialogLink = QLatin1String("SupportDialog");
}

NewWalletDialog::NewWalletDialog(bool noWalletsFound, const std::shared_ptr<ApplicationSettings>& appSettings, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::NewWalletDialog)
{
   ui_->setupUi(this);

   auto netType = appSettings->get<NetworkType>(ApplicationSettings::netType);

   if (noWalletsFound) {
      if (netType == NetworkType::TestNet) {
         ui_->labelPurpose->setText(tr("THE TERMINAL CAN'T FIND ANY EXISTING TESTNET WALLETS"));
      } else {
         ui_->labelPurpose->setText(tr("THE TERMINAL CAN'T FIND ANY EXISTING WALLETS"));
      }
   } else {
      if (netType == NetworkType::TestNet) {
         ui_->labelPurpose->setText(tr("ADD NEW TESTNET WALLET"));
      } else {
         ui_->labelPurpose->setText(tr("ADD NEW WALLET"));
      }
   }

   const auto messageText =
         tr("For guidance, please consult our ")
         + QStringLiteral("<a href=\"%1\">").arg(kSupportDialogLink)
         + QStringLiteral("<span style=\"text-decoration: underline; color: %1;\">Guides</span></a>")
         .arg(BSMessageBox::kUrlColor);

   ui_->labelMessage->setText(messageText);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, [this] {
      isCreate_ = true;
      accept();
   });
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, [this] {
      isImport_ = true;
      accept();
   });

   connect(ui_->labelMessage, &QLabel::linkActivated, this, [this](const QString & link) {
      reject();

      if (link == kSupportDialogLink) {
         SupportDialog *supportDlg = new SupportDialog(parentWidget());
         supportDlg->setTab(0);
         supportDlg->show();
      }
   });
}

NewWalletDialog::~NewWalletDialog() = default;
