/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "NewWalletDialog.h"
#include "ui_NewWalletDialog.h"
#include "WalletsWidget.h"

#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "InfoDialogs/SupportDialog.h"

namespace {
   const QString kSupportDialogLink = QLatin1String("SupportDialog");
}

NewWalletDialog::NewWalletDialog(bool noWalletsFound
   , const std::shared_ptr<ApplicationSettings>& appSettings
   , QWidget *parent)
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
         tr("Need help? Please consult our ")
         + QStringLiteral("<a href=\"%1\">").arg(kSupportDialogLink)
         + QStringLiteral("<span style=\"text-decoration: underline; color: %1;\">Getting Started Guides</span></a>")
         .arg(BSMessageBox::kUrlColor);

   ui_->labelMessage->setText(messageText);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, [this] {
      done(CreateNew);
   });
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, [this] {
      done(ImportExisting);
   });
   connect(ui_->pushButtonHw, &QPushButton::clicked, this, [this] {
      done(ImportHw);
   });
   connect(ui_->labelMessage, &QLabel::linkActivated, this, &NewWalletDialog::onLinkActivated);
}

NewWalletDialog::NewWalletDialog(bool noWalletsFound
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::NewWalletDialog)
{
   ui_->setupUi(this);

   if (noWalletsFound) {
      ui_->labelPurpose->setText(tr("THE TERMINAL CAN'T FIND ANY EXISTING WALLET"));
   } else {
      ui_->labelPurpose->setText(tr("ADD NEW WALLET"));
   }

   const auto messageText =
      tr("Need help? Please consult our ")
      + QStringLiteral("<a href=\"%1\">").arg(kSupportDialogLink)
      + QStringLiteral("<span style=\"text-decoration: underline; color: %1;\">Getting Started Guides</span></a>")
      .arg(BSMessageBox::kUrlColor);

   ui_->labelMessage->setText(messageText);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, [this] {
      done(CreateNew);
   });
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, [this] {
      done(ImportExisting);
   });
   connect(ui_->pushButtonHw, &QPushButton::clicked, this, [this] {
      done(ImportHw);
   });
   connect(ui_->labelMessage, &QLabel::linkActivated, this, &NewWalletDialog::onLinkActivated);
}

NewWalletDialog::~NewWalletDialog() = default;

void NewWalletDialog::onLinkActivated(const QString& link)
{
   if (link == kSupportDialogLink) {
      auto* parent = parentWidget();

      SupportDialog* supportDlg = new SupportDialog(parent);
      supportDlg->setTab(0);
      supportDlg->show();

      auto* walletWidget = qobject_cast<WalletsWidget*>(parent);
      if (walletWidget) {  //FIXME: emit signal instead
         connect(supportDlg, &QDialog::finished, walletWidget, [walletWidget]() {
            walletWidget->onNewWallet();
         });
      }
   }
   else {
      ui_->labelPurpose->setText(tr("Unknown link"));
   }
}
