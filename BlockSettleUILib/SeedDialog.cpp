/*

***********************************************************************************
* Copyright (C) 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ui_SeedDialog.h"

#include <QDialogButtonBox>
#include <QToolTip>

#include "SeedDialog.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"

using namespace bs::gui::qt;

SeedDialog::SeedDialog(const std::string& rootId
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::SeedDialog())
   , walletId_(rootId)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonGenSeed, &QPushButton::clicked, this, &SeedDialog::generateSeed);
   connect(ui_->lineEditSeed, &QLineEdit::editingFinished, this, &SeedDialog::onDataAvail);
   connect(ui_->lineEditWalletName, &QLineEdit::editingFinished, this, &SeedDialog::onDataAvail);
   connect(ui_->lineEditWalletDesc, &QLineEdit::editingFinished, this, &SeedDialog::onDataAvail);
   connect(ui_->lineEditPass1, &QLineEdit::editingFinished, this, &SeedDialog::onPasswordEdited);
   connect(ui_->lineEditPass2, &QLineEdit::editingFinished, this, &SeedDialog::onPasswordEdited);

   okButton_ = ui_->buttonBox->button(QDialogButtonBox::StandardButton::Ok);
   if (okButton_) {
      connect(okButton_, &QPushButton::clicked, this, &SeedDialog::onClose);
   }
   okButton_->setEnabled(false);
}

SeedDialog::~SeedDialog() = default;

void SeedDialog::generateSeed()
{
   const auto& seed = CryptoPRNG::generateRandom(32).toHexStr();
   ui_->lineEditSeed->setText(QString::fromStdString(seed));
}

void SeedDialog::onClose()
{
   data_.xpriv = ui_->textEditXPriv->toPlainText().toStdString();
   data_.seed = SecureBinaryData::CreateFromHex(ui_->lineEditSeed->text().toStdString());
   data_.name = ui_->lineEditWalletName->text().toStdString();
   data_.description = ui_->lineEditWalletDesc->text().toStdString();
   data_.password = SecureBinaryData::fromString(ui_->lineEditPass1->text().toStdString());
}

void SeedDialog::onDataAvail()
{
   std::cout << "test\n";
   okButton_->setEnabled(!ui_->lineEditWalletName->text().isEmpty() &&
      (!ui_->lineEditSeed->text().isEmpty() || !ui_->textEditXPriv->toPlainText().isEmpty()));
}

void bs::gui::qt::SeedDialog::onPasswordEdited()
{
   const auto& pass1 = ui_->lineEditPass1->text().toStdString();
   const auto& pass2 = ui_->lineEditPass2->text().toStdString();
   bool isValid = false;
   if (!pass1.empty() && pass2.empty()) {
      ui_->labelPass->setText(tr("enter same password in the second line"));
   }
   else if (pass1.empty() && pass2.empty()) {
      ui_->labelPass->setText(tr("type password twice"));
   }
   else if (pass1 != pass2) {
      ui_->labelPass->setText(tr("passwords don't match"));
   }
   else {
      ui_->labelPass->clear();
      isValid = true;
   }
   okButton_->setEnabled(isValid);
}

void SeedDialog::showEvent(QShowEvent* event)
{
   QDialog::showEvent(event);
}
