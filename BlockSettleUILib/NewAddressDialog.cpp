#include "NewAddressDialog.h"
#include "ui_NewAddressDialog.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include <QClipboard>
#include <QDialogButtonBox>


NewAddressDialog::NewAddressDialog(const std::shared_ptr<bs::Wallet>& wallet
   , const std::shared_ptr<SignContainer> &container, bool isNested, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::NewAddressDialog())
   , wallet_(wallet)
   , address_(wallet_->GetNewExtAddress(isNested ? AddressEntryType_P2SH : AddressEntryType_P2WPKH))
{
   container->SyncAddresses({ {wallet_, address_} });
   wallet_->RegisterWallet();

   ui_->setupUi(this);
   ui_->labelWallet->setText(QString::fromStdString(wallet->GetWalletName()));

   QString displayAddress = address_.display();
   ui_->lineEditNewAddress->setText(displayAddress);
   ui_->labelQRCode->setPixmap(UiUtils::getQRCode(displayAddress));

   auto copyButton = ui_->buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
   connect(copyButton, &QPushButton::clicked, this, &NewAddressDialog::copyToClipboard);
   connect(ui_->pushButtonCopyToClipboard, &QPushButton::clicked, this, &NewAddressDialog::copyToClipboard);

   connect(this, &QDialog::accepted, [this] {
      const auto comment = ui_->textEditDescription->toPlainText();
      if (!comment.isEmpty()) {
         wallet_->SetAddressComment(address_, comment.toStdString());
      }
   });
}

void NewAddressDialog::copyToClipboard()
{
   QApplication::clipboard()->setText(address_.display());
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
   auto leftMargin = ui_->lineEditNewAddress->textMargins().left();
   auto rightMargin = ui_->lineEditNewAddress->textMargins().right();

   layout()->activate();
}