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
#include "bip39.h"
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
   setWindowTitle(tr("Create new wallet"));
   ui_->lineEditSeed->setReadOnly(true);

   connect(ui_->pushButtonGenSeed, &QPushButton::clicked, this, &SeedDialog::generateSeed);
   connect(ui_->lineEditSeed, &QLineEdit::textChanged, this, &SeedDialog::onDataAvail);
   connect(ui_->lineEditWalletName, &QLineEdit::editingFinished, this, &SeedDialog::onDataAvail);
   connect(ui_->lineEditWalletDesc, &QLineEdit::editingFinished, this, &SeedDialog::onDataAvail);
   connect(ui_->lineEditPass1, &QLineEdit::editingFinished, this, &SeedDialog::onPasswordEdited);
   connect(ui_->lineEditPass2, &QLineEdit::editingFinished, this, &SeedDialog::onPasswordEdited);
   connect(ui_->textEdit12Words, &QTextEdit::textChanged, this, &SeedDialog::on12WordsChanged);

   okButton_ = ui_->buttonBox->button(QDialogButtonBox::StandardButton::Ok);
   if (okButton_) {
      connect(okButton_, &QPushButton::clicked, this, &SeedDialog::onClose);
   }
   okButton_->setEnabled(false);
}

SeedDialog::~SeedDialog() = default;

void SeedDialog::generateSeed()
{
   auto seed = CryptoPRNG::generateRandom(16);
   std::vector<uint8_t> seedData;
   for (int i = 0; i < (int)seed.getSize(); ++i) {
      seedData.push_back(seed.getPtr()[i]);
   }
   const auto& words = BIP39::create_mnemonic(seedData);
   ui_->textEdit12Words->blockSignals(true);
   ui_->textEdit12Words->setText(QString::fromStdString(words.to_string()));
   ui_->textEdit12Words->blockSignals(false);

   seed = BIP39::seed_from_mnemonic(words);
   ui_->lineEditSeed->setText(QString::fromStdString(seed.toHexStr()));
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

void SeedDialog::onPasswordEdited()
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

void SeedDialog::on12WordsChanged()
{
   const auto& words = ui_->textEdit12Words->toPlainText().split(QLatin1Char(' ')
      , QString::SkipEmptyParts);
   BIP39::word_list wordList;
   for (const auto& word : words) {
      wordList.add(word.toStdString());
   }
   if (BIP39::valid_mnemonic(wordList)) {
      ui_->labelPass->clear();
   } else {
      ui_->labelPass->setText(tr("invalid 12-word seed"));
      ui_->lineEditSeed->clear();
      return;
   }

   const auto& seed = BIP39::seed_from_mnemonic(wordList);

   ui_->lineEditSeed->setText(QString::fromStdString(seed.toHexStr()));
   onDataAvail();
}

void SeedDialog::showEvent(QShowEvent* event)
{
   QDialog::showEvent(event);
}
