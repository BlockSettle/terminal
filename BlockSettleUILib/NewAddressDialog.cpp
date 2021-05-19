/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ui_NewAddressDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QToolTip>

#include "NewAddressDialog.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"


NewAddressDialog::NewAddressDialog(const std::shared_ptr<bs::sync::Wallet> &wallet
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::NewAddressDialog())
   , wallet_(wallet)
{
   ui_->setupUi(this);
   ui_->labelWallet->setText(QString::fromStdString(wallet->name()));

   auto copyButton = ui_->buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
   connect(copyButton, &QPushButton::clicked, this, &NewAddressDialog::copyToClipboard);
   connect(ui_->pushButtonCopyToClipboard, &QPushButton::clicked, this, &NewAddressDialog::copyToClipboard);

   const auto closeButton = ui_->buttonBox->button(QDialogButtonBox::StandardButton::Close);
   if (closeButton) {
      connect(closeButton, &QPushButton::clicked, this, &NewAddressDialog::onClose);
   }

   const auto &cbAddr = [this, copyButton, closeButton](const bs::Address &addr) {
      if (addr.isValid()) {
         address_ = addr;
         QMetaObject::invokeMethod(this, [this, copyButton, closeButton] {
            closeButton->setEnabled(true);
            displayAddress();
            copyButton->setEnabled(true);
         });
         wallet_->syncAddresses();
      }
      else {
         QMetaObject::invokeMethod(this, [this] {
            ui_->lineEditNewAddress->setText(tr("Invalid address"));
         });
      }
   };
   wallet_->getNewExtAddress(cbAddr);

   if (address_.empty()) {
      copyButton->setEnabled(false);
      closeButton->setEnabled(false);
   }
   else {
      displayAddress();
   }
}

NewAddressDialog::NewAddressDialog(const bs::sync::WalletInfo &wallet
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::NewAddressDialog())
   , walletId_(*wallet.ids.cbegin())
{
   ui_->setupUi(this);
   ui_->labelWallet->setText(QString::fromStdString(wallet.name));

   copyButton_ = ui_->buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
   connect(copyButton_, &QPushButton::clicked, this, &NewAddressDialog::copyToClipboard);
   connect(ui_->pushButtonCopyToClipboard, &QPushButton::clicked, this, &NewAddressDialog::copyToClipboard);

   closeButton_ = ui_->buttonBox->button(QDialogButtonBox::StandardButton::Close);
   if (closeButton_) {
      connect(closeButton_, &QPushButton::clicked, this, &NewAddressDialog::onClose);
   }
   copyButton_->setEnabled(false);
   closeButton_->setEnabled(false);
}

NewAddressDialog::~NewAddressDialog() = default;

void NewAddressDialog::onAddresses(const std::string &walletId
   , const std::vector<bs::sync::Address>& addrs)
{
   if (walletId != walletId_) {
      return;  // not our addresses
   }
   address_ = addrs.rbegin()->address; // get last address
   closeButton_->setEnabled(true);
   copyButton_->setEnabled(!address_.empty());
   displayAddress();
}

void NewAddressDialog::displayAddress()
{
   const auto dispAddress = QString::fromStdString(address_.display());
   ui_->lineEditNewAddress->setText(dispAddress);
   const QString addrURI = QLatin1String("bitcoin:") + dispAddress;
   ui_->labelQRCode->setPixmap(UiUtils::getQRCode(addrURI, 128));
}

void NewAddressDialog::copyToClipboard()
{
   auto addr = QString::fromStdString(address_.display());
   QApplication::clipboard()->setText(addr);
   QToolTip::showText(ui_->lineEditNewAddress->mapToGlobal(QPoint(0, 3)), tr("Copied '%1' to clipboard.").arg(addr), this);
}

void NewAddressDialog::onClose()
{
   const auto comment = ui_->textEditDescription->toPlainText();
   if (!comment.isEmpty()) {
      wallet_->setAddressComment(address_, comment.toStdString());
   }
}

void NewAddressDialog::showEvent(QShowEvent* event)
{
   UpdateSizeToAddress();
   QDialog::showEvent(event);
}

void NewAddressDialog::UpdateSizeToAddress()
{
   QString addressText = ui_->lineEditNewAddress->text();

   QFontMetrics fm{ ui_->lineEditNewAddress->font() };

   layout()->activate();
   auto currentWidth = ui_->lineEditNewAddress->width();
   auto textWidth = fm.width(addressText) + fm.descent() + fm.ascent();

   if (currentWidth >= textWidth) {
      return;
   }

   ui_->lineEditNewAddress->setMinimumWidth(textWidth);

   layout()->activate();
}
